//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import Alamofire
import Foundation
import NextcloudFileProviderKit
@testable import NextcloudKit
import Testing

@Suite("Account sessions")
struct AccountSessionTests {
    @Test("Installing credentials clears only that account's persisted errors.")
    func installingCredentialsClearsPersistedErrors() throws {
        let (kit, defaults, suiteName) = try makeKit()
        defer { defaults.removePersistentDomain(forName: suiteName) }
        let account = makeAccount()
        let otherAccounts = ["other-user https://example.invalid", "another-user https://example.invalid"]

        for key in errorKeys(for: kit) {
            defaults.set([account.ncKitAccount, otherAccounts[0], account.ncKitAccount, otherAccounts[1]], forKey: key)
        }

        kit.appendSession(account: account, userAgent: "FileProviderUIExt-test")

        for key in errorKeys(for: kit) {
            #expect(defaults.array(forKey: key) as? [String] == otherAccounts)
        }
        #expect(kit.nkCommonInstance.groupIdentifier == suiteName)
        let session = try #require(kit.nkCommonInstance.nksessions.session(forAccount: account.ncKitAccount))
        #expect(session.urlBase == account.serverUrl)
        #expect(session.user == account.username)
        #expect(session.userId == account.id)
        #expect(session.password == account.password)
        #expect(session.userAgent == "FileProviderUIExt-test")
    }

    @Test("Replacing credentials updates an existing session and clears its errors.")
    func replacingCredentialsRecoversExistingSession() throws {
        let (kit, defaults, suiteName) = try makeKit()
        defer { defaults.removePersistentDomain(forName: suiteName) }
        let original = makeAccount()
        kit.appendSession(account: original, userAgent: "old-user-agent")

        for key in errorKeys(for: kit) {
            defaults.set([original.ncKitAccount], forKey: key)
        }
        let replacement = Account(
            user: original.username,
            id: "updated-user-id",
            serverUrl: original.serverUrl,
            password: "replacement-app-password"
        )

        kit.appendSession(account: replacement, userAgent: "updated-user-agent")

        let session = try #require(kit.nkCommonInstance.nksessions.session(forAccount: original.ncKitAccount))
        #expect(session.password == replacement.password)
        #expect(session.userId == replacement.id)
        #expect(session.userAgent == "updated-user-agent")
        for key in errorKeys(for: kit) {
            #expect(defaults.array(forKey: key) as? [String] == [])
        }
    }

    @Test("Missing or malformed error lists do not prevent session setup.", arguments: [false, true])
    func absentOrMalformedErrorLists(malformed: Bool) throws {
        let (kit, defaults, suiteName) = try makeKit()
        defer { defaults.removePersistentDomain(forName: suiteName) }
        let account = makeAccount()

        if malformed {
            for key in errorKeys(for: kit) {
                defaults.set("invalid-error-list", forKey: key)
            }
        }

        kit.appendSession(account: account, userAgent: "FileProviderUIExt-test")

        let session = try #require(kit.nkCommonInstance.nksessions.session(forAccount: account.ncKitAccount))
        #expect(session.password == account.password)
        #expect(kit.nkCommonInstance.groupIdentifier == suiteName)
    }

    @Test("Session setup clears the receiving SDK instance's defaults suite.")
    func errorResetIsIsolatedToInstance() throws {
        let (firstKit, firstDefaults, firstSuiteName) = try makeKit()
        defer { firstDefaults.removePersistentDomain(forName: firstSuiteName) }
        let (secondKit, secondDefaults, secondSuiteName) = try makeKit()
        defer { secondDefaults.removePersistentDomain(forName: secondSuiteName) }
        let account = makeAccount()

        for key in errorKeys(for: firstKit) {
            firstDefaults.set([account.ncKitAccount], forKey: key)
            secondDefaults.set([account.ncKitAccount], forKey: key)
        }

        firstKit.appendSession(account: account, userAgent: "FileProviderUIExt-test")

        for key in errorKeys(for: firstKit) {
            #expect(firstDefaults.array(forKey: key) as? [String] == [])
            #expect(secondDefaults.array(forKey: key) as? [String] == [account.ncKitAccount])
        }
        #expect(secondKit.nkCommonInstance.nksessions.session(forAccount: account.ncKitAccount) == nil)
    }

    @Test("Requests with replacement credentials preserve server success and failure.", arguments: [200, 401])
    func capabilitiesRequestUsesReplacementCredentials(statusCode: Int) async throws {
        let (kit, defaults, suiteName) = try makeKit()
        defer { defaults.removePersistentDomain(forName: suiteName) }
        let account = makeAccount()
        let capabilitiesURL = try #require(URL(string: account.serverUrl + "/ocs/v1.php/cloud/capabilities"))
        let body = statusCode == 200
            ? Data(#"{"ocs":{"meta":{"status":"ok","statuscode":100,"message":"OK"},"data":{"version":{"string":"32.0.5","minor":0,"micro":5,"major":32},"capabilities":{}}}}"#.utf8)
            : Data(#"{"ocs":{"meta":{"status":"failure","statuscode":997,"message":"Unauthorised"},"data":[]}}"#.utf8)
        MockHTTPURLProtocol.setResponse(for: capabilitiesURL, statusCode: statusCode, body: body)
        defer { MockHTTPURLProtocol.removeResponse(for: capabilitiesURL) }
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MockHTTPURLProtocol.self]
        let session = NKSession(
            nkCommonInstance: kit.nkCommonInstance,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id,
            password: "obsolete-app-password",
            account: account.ncKitAccount,
            userAgent: "old-user-agent",
            groupIdentifier: "",
            httpMaximumConnectionsPerHost: 1,
            httpMaximumConnectionsPerHostInDownload: 1,
            httpMaximumConnectionsPerHostInUpload: 1,
            sessionDataOverride: Alamofire.Session(configuration: configuration)
        )
        kit.nkCommonInstance.nksessions.append(session)
        defaults.set([account.ncKitAccount], forKey: kit.nkCommonInstance.groupDefaultsUnauthorized)

        kit.appendSession(account: account, userAgent: "FileProviderUIExt-test")

        #expect(defaults.array(forKey: kit.nkCommonInstance.groupDefaultsUnauthorized) as? [String] == [])
        let result = await kit.getCapabilitiesAsync(account: account.ncKitAccount)
        #expect(result.responseData?.response?.statusCode == statusCode)
        #expect((result.error == .success) == (statusCode == 200))
        let requests = MockHTTPURLProtocol.requests(for: capabilitiesURL)
        #expect(requests.count == 1)
        let request = try #require(requests.first)
        #expect(request.httpMethod == "GET")
        #expect(request.url == capabilitiesURL)
        let authorization = "Basic " + Data("\(account.username):\(account.password)".utf8).base64EncodedString()
        #expect(request.value(forHTTPHeaderField: "Authorization") == authorization)
        #expect(request.value(forHTTPHeaderField: "User-Agent") == "FileProviderUIExt-test")
        #expect(request.value(forHTTPHeaderField: "OCS-APIRequest") == "true")
    }

    private func makeKit() throws -> (NextcloudKit, UserDefaults, String) {
        let suiteName = "com.nextcloud.desktopclient.tests.AccountSessionTests.\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suiteName))
        let kit = NextcloudKit()
        kit.nkCommonInstance.groupIdentifier = suiteName
        return (kit, defaults, suiteName)
    }

    private func makeAccount() -> Account {
        Account(
            user: "session-user",
            id: "session-user-id",
            serverUrl: "https://\(UUID().uuidString.lowercased()).example.invalid",
            password: "app-password"
        )
    }

    private func errorKeys(for kit: NextcloudKit) -> [String] {
        let common = kit.nkCommonInstance
        return [common.groupDefaultsUnauthorized, common.groupDefaultsUnavailable, common.groupDefaultsToS]
    }
}

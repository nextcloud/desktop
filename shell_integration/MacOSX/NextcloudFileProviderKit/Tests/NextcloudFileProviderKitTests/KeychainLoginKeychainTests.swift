//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import Alamofire
import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
@testable import NextcloudKit
import Testing

///
/// Coverage for ``Keychain`` against the real default keychain, which is the only place where macOS ignores internet password attributes on generic passwords.
///
/// Each test uses random services and a random account, plants a decoy item of another "application" with the same account, and removes everything again afterwards.
///
@Suite(
    "Keychain in the default keychain",
    .enabled(
        if: LoginKeychainFixture.isUsable() || LoginKeychainFixture.isRequired,
        "The default keychain is not usable. Set NEXTCLOUD_REQUIRE_LOGIN_KEYCHAIN_TESTS=1 to fail instead of skipping."
    ),
    .timeLimit(.minutes(1))
)
final class KeychainLoginKeychainTests: Sendable {
    let account = "nextcloud-test-\(UUID().uuidString)"
    let renamedAccount = "nextcloud-test-\(UUID().uuidString)"
    let service = UUID().uuidString
    let otherService = UUID().uuidString
    let decoyService = "nextcloud-test-decoy-\(UUID().uuidString)"

    init() throws {
        try #require(LoginKeychainFixture.isUsable(), "The default keychain is not usable.")
    }

    deinit {
        for service in [service, otherService, decoyService] {
            LoginKeychainFixture.removeAll(service: service)
        }

        // Also catches items stored without a service, as the code before the fix did.
        for account in [account, renamedAccount] {
            LoginKeychainFixture.removeAll(account: account)
        }
    }

    private func makeKeychain(service: String, label: String = LoginKeychainFixture.label) -> Keychain {
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(service), displayName: label)

        return Keychain(domain: domain, log: FileProviderLogMock())
    }

    private func plantDecoy() throws {
        try #require(LoginKeychainFixture.plant(service: decoyService, account: account, password: "decoy-secret", label: "decoy") == errSecSuccess)
    }

    @Test func savedPasswordReadsBackAndUpdatesInPlace() {
        let keychain = makeKeychain(service: service)

        #expect(keychain.savePassword("one", for: account))
        #expect(keychain.getPassword(for: account) == "one")
        #expect(keychain.savePassword("two", for: account))
        #expect(keychain.getPassword(for: account) == "two")
        #expect(LoginKeychainFixture.count(service: service) == 1)
    }

    @Test func savingLeavesForeignItemWithSameAccountUntouched() throws {
        try plantDecoy()
        let keychain = makeKeychain(service: service)

        #expect(keychain.savePassword("mine", for: account))
        #expect(LoginKeychainFixture.password(service: decoyService, account: account) == "decoy-secret")
        #expect(LoginKeychainFixture.label(service: decoyService, account: account) == "decoy")
        #expect(LoginKeychainFixture.count(service: decoyService) == 1)
        #expect(LoginKeychainFixture.count(service: service) == 1)
    }

    @Test func lookupNeverReturnsForeignItemWithSameAccount() throws {
        try plantDecoy()
        let keychain = makeKeychain(service: service)

        #expect(keychain.getPassword(for: account) == nil)
    }

    @Test func domainsWithTheSameUserNameKeepSeparatePasswords() {
        let keychain = makeKeychain(service: service)
        let otherKeychain = makeKeychain(service: otherService)

        #expect(keychain.savePassword("a", for: account))
        #expect(otherKeychain.savePassword("b", for: account))
        #expect(keychain.getPassword(for: account) == "a")
        #expect(otherKeychain.getPassword(for: account) == "b")
        #expect(LoginKeychainFixture.count(service: service) == 1)
        #expect(LoginKeychainFixture.count(service: otherService) == 1)
    }

    @Test func labelFollowsTheDisplayNameOnUpdate() {
        #expect(makeKeychain(service: service, label: "Old").savePassword("one", for: account))
        #expect(makeKeychain(service: service, label: "New").savePassword("two", for: account))
        #expect(LoginKeychainFixture.label(service: service, account: account) == "New")
        #expect(LoginKeychainFixture.count(service: service) == 1)
    }

    @Test func deletingRemovesOnlyItemsOfTheOwnDomain() throws {
        try plantDecoy()
        let keychain = makeKeychain(service: service)
        let otherKeychain = makeKeychain(service: otherService)
        #expect(keychain.savePassword("a", for: account))
        #expect(otherKeychain.savePassword("b", for: account))

        #expect(keychain.deletePassword())
        #expect(LoginKeychainFixture.count(service: service) == 0)
        #expect(otherKeychain.getPassword(for: account) == "b")
        #expect(LoginKeychainFixture.password(service: decoyService, account: account) == "decoy-secret")
    }

    @Test func deletingRemovesItemsOfAnEarlierUserName() {
        let keychain = makeKeychain(service: service)
        #expect(keychain.savePassword("old", for: renamedAccount))
        #expect(keychain.savePassword("new", for: account))

        #expect(keychain.deletePassword())
        #expect(LoginKeychainFixture.count(service: service) == 0)
    }

    @Test func deletingWithNothingStoredSucceeds() {
        #expect(makeKeychain(service: service).deletePassword())
    }

    @Test func removingAccountConfigDeletesTheDomainPassword() throws {
        try plantDecoy()
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(service), displayName: LoginKeychainFixture.label)
        let ext = FileProviderExtension(domain: domain)
        ext.ncAccount = Account(user: account, id: account, serverUrl: "https://mock.nc.com", password: "x")
        #expect(ext.keychain.savePassword("x", for: account))

        ext.removeAccountConfig()

        #expect(LoginKeychainFixture.count(service: service) == 0)
        #expect(ext.ncAccount == nil)
        #expect(LoginKeychainFixture.password(service: decoyService, account: account) == "decoy-secret")
    }

    @Test(arguments: [false, true])
    func removalSupersedesSetupsStartedBeforeIt(signOut: Bool) {
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(service), displayName: LoginKeychainFixture.label)
        let ext = FileProviderExtension(domain: domain)
        let setupAccount = Account(user: account, id: account, serverUrl: "https://mock.nc.com", password: "x")
        let staleGeneration = ext.setupGeneration
        defer { UserDefaults.standard.removeObject(forKey: domain.identifier.rawValue) }
        #expect(ext.storeAccount(setupAccount, generation: staleGeneration, awaitingDatabaseSetup: true))
        #expect(ext.config.accountForSetupRetry != nil)

        if signOut {
            ext.removeAccountConfig()
        } else {
            ext.deletePassword(completionHandler: nil)
        }

        // A setup queued or authenticating before the removal neither stores nor publishes its account.
        #expect(ext.storeAccount(setupAccount, generation: staleGeneration, awaitingDatabaseSetup: true) == false)
        #expect(ext.config.accountForSetupRetry == nil)
        #expect(ext.publishAccount(setupAccount, generation: staleGeneration) == false)
        #expect(LoginKeychainFixture.count(service: service) == 0)
        #expect(ext.ncAccount == nil)

        // A setup started afterwards works as usual.
        #expect(ext.storeAccount(setupAccount, generation: ext.setupGeneration))
        #expect(LoginKeychainFixture.password(service: service, account: account) == "x")
    }

    @Test(arguments: [false, true])
    func finderRetriesNewDAVAccountAfterFailedSetupAndRestart(authenticationSucceeds: Bool) async throws {
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(service), displayName: LoginKeychainFixture.label)
        let previous = Account(
            user: account,
            id: "previous-id",
            serverUrl: "https://" + service.lowercased() + ".previous.invalid",
            password: "previous"
        )
        let current = Account(
            user: account,
            id: "current-id",
            serverUrl: "https://" + service.lowercased() + ".current.invalid",
            password: "current"
        )
        let previousDAVURL = try #require(URL(string: previous.davFilesUrl + "/"))
        let currentDAVURL = try #require(URL(string: current.davFilesUrl + "/"))
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(service, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: false)
        defer { try? FileManager.default.removeItem(at: directory) }
        // Opening SQLite below a regular file fails even after a successful authentication.
        let blockedDatabaseDirectory = directory.appendingPathComponent("blocked")
        try Data().write(to: blockedDatabaseDirectory)
        let expectedError = NSError(authenticationSucceeds ? .databaseUnavailable : .invalidCredentials)
        let multistatus = Data("""
        <d:multistatus xmlns:d="DAV:">
          <d:response>
            <d:href>\(currentDAVURL.path)/</d:href>
            <d:propstat>
              <d:prop><d:resourcetype><d:collection/></d:resourcetype></d:prop>
              <d:status>HTTP/1.1 200 OK</d:status>
            </d:propstat>
          </d:response>
        </d:multistatus>
        """.utf8)
        MockHTTPURLProtocol.setResponse(for: previousDAVURL, statusCode: 401, body: Data())
        MockHTTPURLProtocol.setResponse(
            for: currentDAVURL,
            statusCode: authenticationSucceeds ? 207 : 401,
            body: authenticationSucceeds ? multistatus : Data()
        )
        defer {
            MockHTTPURLProtocol.removeResponse(for: previousDAVURL)
            MockHTTPURLProtocol.removeResponse(for: currentDAVURL)
            UserDefaults.standard.removeObject(forKey: domain.identifier.rawValue)
        }

        let remote = NextcloudKit()
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MockHTTPURLProtocol.self]
        let sessions = [previous, current].map { identity in
            NKSession(
                nkCommonInstance: remote.nkCommonInstance,
                urlBase: identity.serverUrl,
                user: identity.username,
                userId: identity.id,
                password: "replace this in setup",
                account: identity.ncKitAccount,
                userAgent: "FileProvider-test",
                groupIdentifier: "",
                httpMaximumConnectionsPerHost: 1,
                httpMaximumConnectionsPerHostInDownload: 1,
                httpMaximumConnectionsPerHostInUpload: 1,
                sessionDataOverride: Alamofire.Session(configuration: configuration)
            )
        }
        remote.nkCommonInstance.nksessions = SynchronizedNKSessionArray(sessions)

        let provider = FileProviderExtension(domain: domain, ncKit: remote, databaseDirectory: blockedDatabaseDirectory)
        defer { provider.invalidate() }
        #expect(provider.storeAccount(previous, generation: provider.setupGeneration))

        let firstError: NSError? = await withCheckedContinuation { continuation in
            provider.setupDomainAccount(
                user: current.username,
                userId: current.id,
                serverUrl: current.serverUrl,
                password: current.password
            ) { error in
                continuation.resume(returning: error)
            }
        }
        try #require(firstError == expectedError)
        #expect(provider.config.serverUrl == previous.serverUrl)
        #expect(provider.config.userId == previous.id)
        #expect(provider.storedAccountForAuthentication() == current)
        try #require(MockHTTPURLProtocol.requests(for: currentDAVURL).count == 1)

        let immediateRetryError = await FPUIExtensionServiceSource(fpExtension: provider).authenticate()
        #expect(immediateRetryError == expectedError)

        let restartedProvider = FileProviderExtension(domain: domain, ncKit: remote, databaseDirectory: blockedDatabaseDirectory)
        defer { restartedProvider.invalidate() }
        let retryError = await FPUIExtensionServiceSource(fpExtension: restartedProvider).authenticate()
        #expect(retryError == expectedError)

        let retryRequests = MockHTTPURLProtocol.requests(for: currentDAVURL)
        #expect(retryRequests.map(\.httpMethod) == ["PROPFIND", "PROPFIND", "PROPFIND"])
        let credentials = Data((current.username + ":" + current.password).utf8).base64EncodedString()
        #expect(retryRequests.last?.value(forHTTPHeaderField: "Authorization") == "Basic " + credentials)
        #expect(MockHTTPURLProtocol.requests(for: previousDAVURL).isEmpty)
        #expect(restartedProvider.config.serverUrl == previous.serverUrl)
        #expect(restartedProvider.config.userId == previous.id)
    }

    @Test func stagingAnAccountKeepsPriorSettingsUntilCommit() {
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(service), displayName: LoginKeychainFixture.label)
        let ext = FileProviderExtension(domain: domain)
        let previous = Account(user: account, id: account, serverUrl: "https://old.example.com", password: "old")
        let current = Account(user: account, id: account, serverUrl: "https://new.example.com", password: "new")
        let generation = ext.setupGeneration
        defer { UserDefaults.standard.removeObject(forKey: domain.identifier.rawValue) }

        #expect(ext.storeAccount(previous, generation: generation, awaitingDatabaseSetup: true))
        #expect(ext.config.serverUrl == nil)
        #expect(ext.storedAccountForAuthentication() == previous)

        #expect(ext.storeAccount(previous, generation: generation))
        #expect(ext.config.serverUrl == previous.serverUrl)
        #expect(ext.config.accountForSetupRetry == nil)

        #expect(ext.storeAccount(current, generation: generation, awaitingDatabaseSetup: true))
        #expect(ext.config.serverUrl == previous.serverUrl)
        #expect(ext.storedAccountForAuthentication() == current)

        #expect(ext.storeAccount(current, generation: generation))
        #expect(ext.config.serverUrl == current.serverUrl)
        #expect(ext.config.accountForSetupRetry == nil)
        #expect(ext.storedAccountForAuthentication() == current)
    }

    @Test func deletePasswordRequestRepliesAfterDeletion() throws {
        try plantDecoy()
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(service), displayName: LoginKeychainFixture.label)
        let ext = FileProviderExtension(domain: domain)
        #expect(ext.keychain.savePassword("x", for: account))
        var reply: Bool?

        ext.deletePassword { reply = $0 }

        #expect(reply == true)
        #expect(LoginKeychainFixture.count(service: service) == 0)
        #expect(LoginKeychainFixture.password(service: decoyService, account: account) == "decoy-secret")
    }

    @Test func emptyPasswordIsNotSaved() {
        let keychain = makeKeychain(service: service)

        #expect(keychain.savePassword("", for: account) == false)
        #expect(LoginKeychainFixture.count(service: service) == 0)
    }
}

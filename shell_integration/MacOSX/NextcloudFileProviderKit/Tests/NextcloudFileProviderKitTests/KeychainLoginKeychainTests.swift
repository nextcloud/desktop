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
    /// The test alone completes the held response after checking the old account.
    private struct HeldValidationResponse: @unchecked Sendable {
        let request: URLRequest
        let respond: () -> Void
    }

    enum IncomingAccountFailure: Int {
        case rejectedCredentials = 401
        case unavailableServer = 502
        case unavailableDatabase = 207

        var expectedError: NSError {
            switch self {
                case .rejectedCredentials: NSError(.invalidCredentials)
                case .unavailableServer: NSError(.connection)
                case .unavailableDatabase: NSError(.databaseUnavailable)
            }
        }

        var attemptsPerSetup: Int { self == .unavailableServer ? 2 : 1 }
    }

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
        let configuredAccount = Account(user: account, id: account, serverUrl: "https://mock.nc.com", password: "x")
        let databaseManager = DatabaseTestSuites.makeManager(account: configuredAccount)
        try #require(ext.publishDatabaseManager(databaseManager, generation: ext.setupGeneration))
        #expect(ext.keychain.savePassword("x", for: account))

        ext.removeAccountConfig()

        #expect(LoginKeychainFixture.count(service: service) == 0)
        #expect(ext.dbManager == nil)
        #expect(!databaseManager.syncOperations.isAcceptingWork)
        #expect(LoginKeychainFixture.password(service: decoyService, account: account) == "decoy-secret")
    }

    @Test(arguments: [false, true])
    func removalSupersedesSetupsStartedBeforeIt(signOut: Bool) {
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(service), displayName: LoginKeychainFixture.label)
        let ext = FileProviderExtension(domain: domain)
        let setupAccount = Account(user: account, id: account, serverUrl: "https://mock.nc.com", password: "x")
        let databaseManager = DatabaseTestSuites.makeManager(account: setupAccount)
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
        #expect(ext.publishDatabaseManager(databaseManager, generation: staleGeneration) == false)
        #expect(LoginKeychainFixture.count(service: service) == 0)
        #expect(ext.dbManager == nil)

        // A setup started afterwards works as usual.
        #expect(ext.storeAccount(setupAccount, generation: ext.setupGeneration))
        #expect(LoginKeychainFixture.password(service: service, account: account) == "x")
    }

    private static func filesRootResponse(at url: URL) -> Data {
        Data("""
        <d:multistatus xmlns:d="DAV:">
          <d:response>
            <d:href>\(url.path)/</d:href>
            <d:propstat>
              <d:prop><d:resourcetype><d:collection/></d:resourcetype></d:prop>
              <d:status>HTTP/1.1 200 OK</d:status>
            </d:propstat>
          </d:response>
        </d:multistatus>
        """.utf8)
    }

    private static func installMockSession(
        in remote: NextcloudKit,
        account: Account,
        key: String,
        userAgent: String = "FileProvider-test"
    ) {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MockHTTPURLProtocol.self]
        remote.nkCommonInstance.nksessions.append(NKSession(
            nkCommonInstance: remote.nkCommonInstance,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id,
            password: account.password,
            account: key,
            userAgent: userAgent,
            groupIdentifier: "",
            httpMaximumConnectionsPerHost: 1,
            httpMaximumConnectionsPerHostInDownload: 1,
            httpMaximumConnectionsPerHostInUpload: 1,
            sessionDataOverride: Alamofire.Session(configuration: configuration)
        ))
    }

    private static func makeMockValidationSession(_ remote: NextcloudKit, _ account: Account, _ userAgent: String) -> String {
        let key = UUID().uuidString
        installMockSession(
            in: remote,
            account: account,
            key: key,
            userAgent: userAgent
        )
        return key
    }

    @Test(arguments: [false, true])
    func accountSetupWaitsForOutstandingSyncWork(afterRedirect: Bool) async throws {
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
            serverUrl: afterRedirect ? "https://" + service.lowercased() + ".current.invalid" : previous.serverUrl,
            password: "current"
        )
        let previousDAVURL = try #require(URL(string: previous.davFilesUrl + "/"))
        let currentDAVURL = try #require(URL(string: current.davFilesUrl + "/"))
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(service, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: false)
        defer {
            try? FileManager.default.removeItem(at: directory)
            UserDefaults.standard.removeObject(forKey: domain.identifier.rawValue)
            MockHTTPURLProtocol.removeResponse(for: previousDAVURL)
            MockHTTPURLProtocol.removeResponse(for: currentDAVURL)
        }

        for url in [previousDAVURL, currentDAVURL] {
            MockHTTPURLProtocol.setResponse(for: url, statusCode: 207, body: Self.filesRootResponse(at: url))
        }
        let remote = NextcloudKit()
        for account in afterRedirect ? [previous, current] : [previous] {
            Self.installMockSession(in: remote, account: account, key: account.ncKitAccount)
        }

        let provider = FileProviderExtension(
            domain: domain,
            ncKit: remote,
            databaseDirectory: directory,
            makeDAVValidationSession: Self.makeMockValidationSession
        )
        defer { provider.invalidate() }
        let previousSetupError: NSError? = await withCheckedContinuation { continuation in
            provider.setupDomainAccount(
                user: previous.username,
                userId: previous.id,
                serverUrl: previous.serverUrl,
                password: previous.password
            ) { error in
                continuation.resume(returning: error)
            }
        }
        try #require(previousSetupError == nil)
        let previousManager = try #require(provider.dbManager)
        if !afterRedirect {
            let activeSession = try #require(remote.nkCommonInstance.nksessions.session(forAccount: previous.ncKitAccount))
            let key = remote.makeDAVValidationSession(account: current, userAgent: "FileProvider-test")
            defer { remote.removeDAVValidationSession(key) }
            let probe = try #require(remote.nkCommonInstance.nksessions.session(forAccount: key))
            #expect(probe.sharedCookieStorage != activeSession.sharedCookieStorage)
            #expect(probe.user == current.username)
            #expect(probe.password == current.password)
        }
        let write = try provider.beginSyncOperation().operation
        try #require(write.beginServerWrite())
        defer { write.finish() }
        let (cancelled, notifyCancellation) = AsyncStream<Void>.makeStream()
        let download = try provider.beginSyncOperation(onStop: {
            notifyCancellation.yield(())
            notifyCancellation.finish()
        }).operation
        defer { download.finish() }

        let (incomingRequests, notifyIncoming) = AsyncStream<HeldValidationResponse>.makeStream()
        let host = try #require(previousDAVURL.host)
        defer {
            notifyIncoming.finish()
            MockHTTPURLProtocol.removeHandlers(forHost: host)
        }
        // The desktop client removes the old account configuration before handing over a redirected URL.
        if afterRedirect {
            provider.removeAccountConfig()
        } else {
            MockHTTPURLProtocol.setHandlers(
                forHost: host,
                start: { http in
                    if http.request.url == currentDAVURL {
                        notifyIncoming.yield(HeldValidationResponse(
                            request: http.request,
                            respond: { http.respond(status: 207, body: Self.filesRootResponse(at: currentDAVURL)) }
                        ))
                    } else {
                        let credentials = Data((previous.username + ":" + previous.password).utf8).base64EncodedString()
                        #expect(http.request.value(forHTTPHeaderField: "Authorization") == "Basic " + credentials)
                        http.respond(status: 207, body: Self.filesRootResponse(at: previousDAVURL))
                    }
                },
                stop: { _ in }
            )
        }
        let (finished, notifyFinished) = AsyncStream<Bool>.makeStream()
        provider.setupDomainAccount(
            user: current.username,
            userId: current.id,
            serverUrl: current.serverUrl,
            password: current.password
        ) { error in
            notifyFinished.yield(error == nil)
            notifyFinished.finish()
        }

        if !afterRedirect {
            let incoming = try #require(try await nextTestValue(from: incomingRequests))
            defer { incoming.respond() }
            let credentials = Data((current.username + ":" + current.password).utf8).base64EncodedString()
            #expect(incoming.request.httpMethod == "PROPFIND")
            #expect(incoming.request.value(forHTTPHeaderField: "Authorization") == "Basic " + credentials)
            #expect(provider.storedAccountForAuthentication() == previous)
            let (manager, operation) = try provider.beginSyncOperation()
            defer { operation.finish() }
            #expect(manager === previousManager)
            let read = await remote.enumerate(
                remotePath: previousDAVURL.absoluteString,
                depth: .target,
                account: manager.account
            )
            #expect(read.error == .success)
        }
        let cancellation: Void? = try await nextTestValue(from: cancelled)
        try #require(cancellation != nil)
        #expect(download.isStopped)
        #expect(!download.beginServerWrite())
        #expect(previousManager.syncOperations.begin() == nil)
        #expect(!previousManager.wasInvalidatedByDAVAccountChange)
        #expect(previousManager.isForCurrentDAVAccount())
        if afterRedirect {
            #expect(MockHTTPURLProtocol.requests(for: currentDAVURL).isEmpty)
        } else {
            #expect(remote.nkCommonInstance.nksessions.session(forAccount: previous.ncKitAccount)?.password == previous.password)
        }

        // The old account still saves server acknowledgements while setup waits.
        var created = DatabaseTestSuites.makeFile(ocId: "created", fileName: "Created.txt", account: previous)
        created.uploaded = true
        previousManager.addItemMetadata(created)

        // A completed download may still be absent from File Provider's snapshot while setup drains work.
        var downloaded = DatabaseTestSuites.makeFile(ocId: "downloaded", fileName: "Downloaded.txt", account: previous)
        downloaded.downloaded = true
        previousManager.addItemMetadata(downloaded)
        previousManager.pendingMaterializations.recordDownloaded(NSFileProviderItemIdentifier(downloaded.ocId))
        write.finish()

        let setupSucceeded = try await nextTestValue(from: finished)
        try #require(setupSucceeded == true)
        let (manager, operation) = try provider.beginSyncOperation()
        defer { operation.finish() }
        #expect(manager.account == current)
        previousManager.syncOperations.stopAcceptingWork()
        #expect(previousManager.wasInvalidatedByDAVAccountChange)
        #expect(!previousManager.isForCurrentDAVAccount())
        #expect(manager.itemMetadata(ocId: created.ocId)?.remotePath() == current.davFilesUrl + "/Created.txt")
        if afterRedirect {
            #expect(MockHTTPURLProtocol.requests(for: currentDAVURL).count == 1)
        } else {
            #expect(manager.writer === previousManager.writer)
        }
        #expect(provider.storedAccountForAuthentication() == current)
        #expect(remote.nkCommonInstance.nksessions.session(forAccount: current.ncKitAccount)?.password == current.password)

        let observer = MaterializedEnumerationObserver(
            account: current,
            dbManager: manager,
            log: FileProviderLogMock(),
            completionHandler: { _, _ in }
        )
        observer.finishEnumerating(upTo: nil)
        #expect(manager.itemMetadata(ocId: downloaded.ocId)?.downloaded == true)
    }

    @Test(arguments: [
        (IncomingAccountFailure.rejectedCredentials, false),
        (.unavailableDatabase, false),
        (.rejectedCredentials, true),
        (.unavailableServer, true),
        (.unavailableDatabase, true)
    ])
    func failedSetupKeepsLiveAccountOrPreservesColdRetry(
        failure: IncomingAccountFailure,
        hasPublishedAccount: Bool
    ) async throws {
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
            serverUrl: hasPublishedAccount ? previous.serverUrl : "https://" + service.lowercased() + ".current.invalid",
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
        MockHTTPURLProtocol.setResponse(
            for: previousDAVURL,
            statusCode: 207,
            body: Self.filesRootResponse(at: previousDAVURL)
        )
        MockHTTPURLProtocol.setResponse(
            for: currentDAVURL,
            statusCode: failure.rawValue,
            body: failure == .unavailableDatabase ? Self.filesRootResponse(at: currentDAVURL) : Data()
        )
        defer {
            MockHTTPURLProtocol.removeResponse(for: previousDAVURL)
            MockHTTPURLProtocol.removeResponse(for: currentDAVURL)
            UserDefaults.standard.removeObject(forKey: domain.identifier.rawValue)
        }

        let remote = NextcloudKit()
        for account in hasPublishedAccount ? [previous] : [previous, current] {
            Self.installMockSession(in: remote, account: account, key: account.ncKitAccount)
        }

        let provider = FileProviderExtension(
            domain: domain,
            ncKit: remote,
            databaseDirectory: hasPublishedAccount ? directory : blockedDatabaseDirectory,
            authenticationRetryDelays: [0],
            makeDAVValidationSession: Self.makeMockValidationSession
        )
        defer { provider.invalidate() }
        #expect(provider.storeAccount(previous, generation: provider.setupGeneration))
        let cachedFile = DatabaseTestSuites.makeFile(ocId: "cached", fileName: "Cached.txt", account: previous)
        if hasPublishedAccount {
            let previousManager = try FilesDatabaseManager(
                account: previous,
                databaseDirectory: directory,
                fileProviderDomainIdentifier: domain.identifier,
                log: FileProviderLogMock()
            )
            try #require(provider.publishDatabaseManager(previousManager, generation: provider.setupGeneration))
            previousManager.addItemMetadata(cachedFile)
            if failure == .unavailableDatabase {
                // An actual SQLite write failure must roll back the account and all item addresses.
                try previousManager.setItemMetadataWriteFailureForTesting(true)
            }
            _ = try provider.enumerator(for: .workingSet, request: NSFileProviderRequest())
        }

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
        try #require(firstError == failure.expectedError)
        let attempts = MockHTTPURLProtocol.requests(for: currentDAVURL)
        let currentCredentials = Data((current.username + ":" + current.password).utf8).base64EncodedString()
        #expect(attempts.count == failure.attemptsPerSetup)
        #expect(attempts.allSatisfy { $0.value(forHTTPHeaderField: "Authorization") == "Basic " + currentCredentials })
        #expect(provider.config.serverUrl == previous.serverUrl)
        #expect(provider.config.userId == previous.id)
        if hasPublishedAccount {
            let (manager, operation) = try provider.beginSyncOperation()
            defer { operation.finish() }
            #expect(manager.account == previous)
            #expect(manager.isForCurrentDAVAccount())
            #expect(manager.itemMetadata(ocId: cachedFile.ocId)?.remotePath() == cachedFile.remotePath())
            #expect(provider.storedAccountForAuthentication() == previous)
            #expect(provider.keychain.getPassword(for: previous.username) == previous.password)
            let result = await remote.enumerate(
                remotePath: previousDAVURL.absoluteString,
                depth: .target,
                account: manager.account
            )
            #expect(result.error == .success)
            let credentials = Data((previous.username + ":" + previous.password).utf8).base64EncodedString()
            #expect(MockHTTPURLProtocol.requests(for: previousDAVURL).last?.value(forHTTPHeaderField: "Authorization") == "Basic " + credentials)
            return
        }
        #expect(throws: NSFileProviderError(.notAuthenticated)) {
            _ = try provider.enumerator(for: .workingSet, request: NSFileProviderRequest())
        }
        #expect(provider.storedAccountForAuthentication() == current)

        let immediateRetryError = await FPUIExtensionServiceSource(fpExtension: provider).authenticate()
        #expect(immediateRetryError == failure.expectedError)

        let restartedProvider = FileProviderExtension(
            domain: domain,
            ncKit: remote,
            databaseDirectory: blockedDatabaseDirectory,
            authenticationRetryDelays: [0]
        )
        defer { restartedProvider.invalidate() }
        let retryError = await FPUIExtensionServiceSource(fpExtension: restartedProvider).authenticate()
        #expect(retryError == failure.expectedError)

        let retryRequests = MockHTTPURLProtocol.requests(for: currentDAVURL)
        #expect(retryRequests.count == failure.attemptsPerSetup * 3)
        #expect(retryRequests.allSatisfy { $0.httpMethod == "PROPFIND" })
        #expect(retryRequests.last?.value(forHTTPHeaderField: "Authorization") == "Basic " + currentCredentials)
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

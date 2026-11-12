//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import Alamofire
@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
@testable import NextcloudKit
import Testing
@testable import TestInterface

@Suite("Enumerator metadata persistence")
struct EnumeratorPersistenceTests {
    private let account: Account
    private let manager: FilesDatabaseManager
    private let folder: MockRemoteItem
    private let file: MockRemoteItem
    private let remote: MockRemoteInterface
    private let anchorDate = Date(timeIntervalSinceNow: -60)

    init() {
        self.init(account: DatabaseTestSuites.account)
    }

    private init(account: Account) {
        self.account = account
        manager = DatabaseTestSuites.makeManager(account: account)
        let root = MockRemoteItem.rootItem(account: account)
        let folder = MockRemoteItem(
            identifier: "folder", versionIdentifier: "folder-v1", name: "folder",
            remotePath: account.davFilesUrl + "/folder", directory: true,
            account: account.ncKitAccount, username: account.username,
            userId: account.id, serverUrl: account.serverUrl
        )
        let file = MockRemoteItem(
            identifier: "file", versionIdentifier: "file-v1", name: "file.txt",
            remotePath: folder.remotePath + "/file.txt",
            account: account.ncKitAccount, username: account.username,
            userId: account.id, serverUrl: account.serverUrl
        )
        folder.parent = root
        root.children = [folder]
        file.parent = folder
        folder.children = [file]
        self.folder = folder
        self.file = file
        remote = MockRemoteInterface(account: account, rootItem: root)

        var folderMetadata = folder.toItemMetadata(account: account)
        folderMetadata.visitedDirectory = true
        folderMetadata.syncTime = Date(timeIntervalSinceNow: -120)
        manager.addItemMetadata(folderMetadata)
        var fileMetadata = file.toItemMetadata(account: account)
        fileMetadata.syncTime = folderMetadata.syncTime
        manager.addItemMetadata(fileMetadata)
    }

    @Test func failedWriteReturnsAnErrorWithoutUnpersistedMetadata() async throws {
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(true)

        let result = await readFolder()

        #expect(remote.readOperationCount == 1)
        #expect(result.error == .metadataDatabaseWriteFailed)
        #expect(result.error?.fileProviderError?.code == .cannotSynchronize)
        #expect(result.metadatas == nil)
        #expect(result.changes == nil)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v1")
    }

    @Test func workingSetKeepsItsAnchorAndReportsTheChangeAfterRetry() async throws {
        folder.versionIdentifier = "folder-v2"
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(true)
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        let failed = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))

        try await failed.enumerateChanges(from: anchor)

        #expect(failed.error == nil)
        #expect(failed.changedItems.isEmpty)
        #expect(failed.deletedItemIdentifiers.isEmpty)
        #expect(failed.finishes.last?.anchor.rawValue == anchor.rawValue)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v1")

        try manager.setItemMetadataWriteFailureForTesting(false)
        let recovered = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))
        try await recovered.enumerateChanges(from: anchor)

        #expect(recovered.error == nil)
        #expect(recovered.changedItems.contains { $0.itemIdentifier.rawValue == file.identifier })
        #expect(recovered.deletedItemIdentifiers.isEmpty)
        let recoveredAnchor = try #require(recovered.finishes.last?.anchor)
        #expect(recoveredAnchor.rawValue != anchor.rawValue)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v2")
    }

    @Test func containerChangeEnumerationReportsThePersistenceError() async throws {
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(true)
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.init(folder.identifier)))

        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity))
        }

        let error = try #require(observer.error) as NSError
        #expect(error.domain == NSFileProviderErrorDomain)
        #expect(error.code == NSFileProviderError.Code.cannotSynchronize.rawValue)
        #expect(observer.finishes.isEmpty)
        #expect(observer.changedItems.isEmpty)
        #expect(observer.deletedItemIdentifiers.isEmpty)
    }

    @Test func unchangedDirectoryReturnsAnEmptyChangeSet() async throws {
        let result = await readFolder()

        #expect(result.error == nil)
        #expect(result.metadatas?.count == 2)
        let changes = try #require(result.changes)
        #expect(changes.created.isEmpty)
        #expect(changes.updated.isEmpty)
        #expect(changes.deleted.isEmpty)
    }

    @Test func successfulWritePersistsAndReportsTheChangedFile() async throws {
        file.versionIdentifier = "file-v2"

        let result = await readFolder()

        #expect(result.error == nil)
        let changes = try #require(result.changes)
        #expect(changes.updated.contains { $0.ocId == file.identifier && $0.etag == "file-v2" })
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v2")
    }

    @Test func directoryListingPreservesChildrenStoredUnderTheDAVResponseURL() async throws {
        let account = Account(
            user: "login",
            id: "backend-id",
            serverUrl: "https://example.com/instance//nextcloud",
            password: "test"
        )
        let responseFilesRoot = "https://example.com/instance/nextcloud" + Account.webDavFilesUrlSuffix + account.id
        let responseDirectoryURL = responseFilesRoot + "/Docs"
        let requestedDirectoryURL = account.davFilesUrl + "/Docs"
        let manager = DatabaseTestSuites.makeManager(account: account)
        var cached = DatabaseTestSuites.makeFile(
            ocId: "report-id",
            fileName: "Report.txt",
            serverUrl: responseDirectoryURL,
            account: account
        )
        cached.uploaded = true
        cached.downloaded = true
        cached.keepDownloaded = true
        cached.etag = "old-etag"
        try manager.insertForTesting(cached)

        var directory = NKFile()
        directory.ocId = "docs-id"
        directory.directory = true
        directory.fileName = "Docs"
        directory.serverUrl = responseFilesRoot
        directory.account = account.ncKitAccount
        directory.urlBase = account.serverUrl
        directory.user = account.username
        directory.userId = account.id

        var report = directory
        report.ocId = cached.ocId
        report.directory = false
        report.fileName = cached.fileName
        report.serverUrl = responseDirectoryURL
        report.etag = "new-etag"

        let result = await Enumerator.handleDepth1ReadFileOrFolder(
            serverUrl: requestedDirectoryURL,
            account: account,
            dbManager: manager,
            files: [directory, report],
            pageIndex: nil,
            preserveDeletedTarget: false,
            log: FileProviderLogMock()
        )

        #expect(result.readError == nil)
        let stored = try #require(manager.itemMetadata(ocId: cached.ocId))
        #expect(stored.etag == report.etag)
        #expect(stored.downloaded)
        #expect(stored.keepDownloaded)
    }

    @Test func serverErrorIsPreserved() async {
        let error = NKError(statusCode: 503, fallbackDescription: "Unavailable")
        remote.enumerateErrorBySuffix = ["/folder": error]

        let result = await readFolder()

        #expect(result.error == error)
        #expect(result.metadatas == nil)
        #expect(result.changes == nil)
    }

    @Test(arguments: [
        ("/instance//nextcloud", "/instance//nextcloud"),
        ("/instance/./nextcloud", "/instance/nextcloud"),
        ("/instance/previous/../nextcloud", "/instance/nextcloud")
    ])
    func readsThePathProducedByTheDAVResponseParser(configuredWebroot: String, responseWebroot: String) async throws {
        let origin = "https://\(UUID().uuidString.lowercased()).example.invalid"
        let account = Account(
            user: "login",
            id: "user-id",
            serverUrl: origin + configuredWebroot,
            password: "test"
        )
        let filePath = Account.webDavFilesUrlSuffix + account.id + "/Docs/report.txt"
        let configuredURL = try #require(URL(string: account.serverUrl + filePath))
        let parsedURL = try #require(URL(string: origin + "/instance/nextcloud" + filePath))
        let ocId = "parser-file-id"
        let body = Data("""
        <d:multistatus xmlns:d="DAV:" xmlns:oc="http://owncloud.org/ns">
          <d:response>
            <d:href>\(responseWebroot)\(filePath)</d:href>
            <d:propstat>
              <d:prop>
                <oc:id>\(ocId)</oc:id>
                <d:getcontenttype>text/plain</d:getcontenttype>
              </d:prop>
              <d:status>HTTP/1.1 200 OK</d:status>
            </d:propstat>
          </d:response>
        </d:multistatus>
        """.utf8)
        MockHTTPURLProtocol.setResponse(for: configuredURL, statusCode: 207, body: body)
        MockHTTPURLProtocol.setResponse(for: parsedURL, statusCode: 207, body: body)
        defer {
            MockHTTPURLProtocol.removeResponse(for: configuredURL)
            MockHTTPURLProtocol.removeResponse(for: parsedURL)
        }

        let remote = nextcloudKitForDAVRead(account: account)
        let initialRead = await remote.enumerate(remotePath: configuredURL.absoluteString, depth: .target, account: account)
        try #require(initialRead.error == .success)
        let file = try #require(initialRead.files.first)
        try #require(file.matchesResponseItemURL(parsedURL.absoluteString))
        let metadata = file.toItemMetadata()
        try #require(metadata.urlBase == account.serverUrl)

        let manager = DatabaseTestSuites.makeManager(account: account)
        let result = await Enumerator.readServerUrl(
            parsedURL.absoluteString,
            account: account,
            sourceMetadata: metadata,
            remoteInterface: remote,
            dbManager: manager,
            depth: .target,
            log: FileProviderLogMock()
        )

        #expect(MockHTTPURLProtocol.requests(for: parsedURL).map(\.httpMethod) == ["PROPFIND"])
        #expect(result.error == nil)
        #expect(result.metadatas?.map(\.ocId) == [ocId])
    }

    @Test(arguments: [(false, true), (true, true), (false, false), (true, false)])
    func directoryReadAcceptsEquivalentWebrootWithMatchingBackendID(filesRoot: Bool, matchingBackendID: Bool) async throws {
        let origin = "https://\(UUID().uuidString.lowercased()).example.invalid"
        let account = Account(
            user: "login",
            id: "backénd",
            serverUrl: origin + "/N%C3%A9xt",
            password: "test"
        )
        let directorySuffix = filesRoot ? "" : "/Docs"
        let directoryURL = account.davFilesUrl + directorySuffix
        let requestURL = try #require(URL(string: directoryURL))
        let responseBackendID = matchingBackendID ? "back%C3%A9nd" : "backe%CC%81nd"
        let responsePath = "/Ne%CC%81xt" + Account.webDavFilesUrlSuffix + responseBackendID + directorySuffix
        let body = Data("""
        <d:multistatus xmlns:d="DAV:" xmlns:oc="http://owncloud.org/ns">
          <d:response>
            <d:href>\(responsePath)/</d:href>
            <d:propstat>
              <d:prop>
                <oc:id>directory-id</oc:id>
                <d:resourcetype><d:collection/></d:resourcetype>
              </d:prop>
              <d:status>HTTP/1.1 200 OK</d:status>
            </d:propstat>
          </d:response>
          <d:response>
            <d:href>\(responsePath)/report.txt</d:href>
            <d:propstat>
              <d:prop>
                <oc:id>file-id</oc:id>
                <d:getcontenttype>text/plain</d:getcontenttype>
              </d:prop>
              <d:status>HTTP/1.1 200 OK</d:status>
            </d:propstat>
          </d:response>
        </d:multistatus>
        """.utf8)
        MockHTTPURLProtocol.setResponse(for: requestURL, statusCode: 207, body: body)
        defer { MockHTTPURLProtocol.removeResponse(for: requestURL) }

        let manager = DatabaseTestSuites.makeManager(account: account)
        let result = await Enumerator.readServerUrl(
            directoryURL,
            account: account,
            remoteInterface: nextcloudKitForDAVRead(account: account),
            dbManager: manager,
            depth: .targetAndDirectChildren,
            log: FileProviderLogMock()
        )

        let requests = MockHTTPURLProtocol.requests(for: requestURL)
        #expect(requests.map(\.httpMethod) == ["PROPFIND"])
        #expect(requests.first?.value(forHTTPHeaderField: "Depth") == "1")
        if matchingBackendID {
            #expect(result.error == nil)
            #expect(result.metadatas?.map(\.ocId) == ["directory-id", "file-id"])
            let storedFile = manager.itemMetadata(locatedAtRemoteUrl: directoryURL + "/report.txt")
            #expect(storedFile?.ocId == "file-id")
            #expect(storedFile?.serverUrl.utf8.elementsEqual(directoryURL.utf8) == true)
        } else {
            #expect(result.error == .invalidData)
            #expect(manager.itemMetadata(ocId: "file-id") == nil)
        }
    }

    @Test func workingSetSkipsAStaleURLThatWouldReturn404() async throws {
        let previousAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://previous.example.com",
            password: account.password
        )
        try await expectWorkingSetToSkip404(at: previousAccount)
    }

    @Test func workingSetSkipsAStaleURLThatMatchesTheDAVResponseRoot() async throws {
        let currentAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: account.serverUrl + "/instance//nextcloud",
            password: account.password
        )
        let previousAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: account.serverUrl + "/instance/nextcloud",
            password: account.password
        )
        let fixture = Self(account: currentAccount)
        try await fixture.expectWorkingSetToSkip404(at: previousAccount)
    }

    private func nextcloudKitForDAVRead(account: Account) -> NextcloudKit {
        let remote = NextcloudKit()
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MockHTTPURLProtocol.self]
        let session = NKSession(
            nkCommonInstance: remote.nkCommonInstance,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id,
            password: account.password,
            account: account.ncKitAccount,
            userAgent: "FileProvider-test",
            groupIdentifier: "",
            httpMaximumConnectionsPerHost: 1,
            httpMaximumConnectionsPerHostInDownload: 1,
            httpMaximumConnectionsPerHostInUpload: 1,
            sessionDataOverride: Alamofire.Session(configuration: configuration)
        )
        remote.nkCommonInstance.nksessions = SynchronizedNKSessionArray([session])
        return remote
    }

    private func expectWorkingSetToSkip404(at previousAccount: Account) async throws {
        try storeFolderAndFile(at: previousAccount)
        let stalePath = previousAccount.davFilesUrl + "/" + folder.name
        let storedFolder = try #require(manager.itemMetadata(ocId: folder.identifier))
        try #require(storedFolder.urlBase == previousAccount.serverUrl)
        try #require(storedFolder.remotePath() == stalePath)
        remote.enumerateErrorBySuffix[stalePath] = NKError(statusCode: 404, fallbackDescription: "File not found")
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))

        try await observer.enumerateChanges(from: anchor)

        #expect(remote.readOperationCount == 0)
        #expect(observer.changedItems.isEmpty)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(observer.finishes.last?.anchor.rawValue == anchor.rawValue)
        #expect(manager.itemMetadata(ocId: folder.identifier)?.deleted == false)
        #expect(manager.itemMetadata(ocId: file.identifier)?.deleted == false)
    }

    @Test func containerSkipsAStaleURLThatWouldReturnAnEmpty207() async throws {
        let previousAccount = Account(
            user: account.username,
            id: account.id + "-previous",
            serverUrl: account.serverUrl,
            password: account.password
        )
        try storeFolderAndFile(at: previousAccount)
        // The mock serves the old collection without its cached child.
        remote.injectMock(previousAccount)
        remote.rootItem?.remotePath = previousAccount.davFilesUrl
        folder.remotePath = previousAccount.davFilesUrl + "/" + folder.name
        folder.userId = previousAccount.id
        folder.children = []
        remote.enumerateResponseStatusCode = 207
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.init(folder.identifier)))

        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity))
        }

        #expect(remote.readOperationCount == 0)
        #expect(observer.changedItems.isEmpty)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(manager.itemMetadata(ocId: folder.identifier)?.deleted == false)
        #expect(manager.itemMetadata(ocId: file.identifier)?.deleted == false)
    }

    @Test(arguments: [NSFileProviderItemIdentifier.rootContainer.rawValue, "server-root-id"])
    func workingSetRetriesA404ForTheFilesRoot(rootID: String) async throws {
        var root = DatabaseTestSuites.makeFile(
            ocId: rootID,
            fileName: SendableItemMetadata.filesRootPlaceholderName,
            serverUrl: account.davFilesUrl,
            account: account
        )
        root.directory = true
        root.visitedDirectory = true
        root.keepDownloaded = true
        root.syncTime = Date(timeIntervalSinceNow: -120)
        manager.addItemMetadata(root)
        remote.enumerateErrorBySuffix[account.davFilesUrl] = NKError(statusCode: 404, fallbackDescription: "File not found")
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))

        try await observer.enumerateChanges(from: anchor)

        #expect(remote.readOperationCount == 2)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(observer.finishes.last?.anchor.rawValue == anchor.rawValue)
        let savedRoot = try #require(manager.itemMetadata(ocId: rootID))
        #expect(!savedRoot.deleted)
        #expect(savedRoot.keepDownloaded)
    }

    @Test func rootContainerReturnsARetryableErrorWhenDAVReturns404() async throws {
        remote.enumerateErrorBySuffix[account.davFilesUrl] = NKError(statusCode: 404, fallbackDescription: "File not found")
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.rootContainer))

        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await observer.enumerateChanges(from: anchor)
        }

        #expect(remote.readOperationCount == 1)
        #expect(observer.deletedItemIdentifiers.isEmpty)
    }

    @Test func workingSetReportsA404UnderTheCurrentDAVBaseAsADeletion() async throws {
        remote.enumerateErrorBySuffix = ["/folder": NKError(statusCode: 404, fallbackDescription: "File not found")]
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))

        try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity))

        #expect(remote.readOperationCount == 1)
        #expect(observer.deletedItemIdentifiers == [.init(folder.identifier)])
    }

    private func storeFolderAndFile(at previousAccount: Account) throws {
        var storedFolder = try #require(manager.itemMetadata(ocId: folder.identifier))
        storedFolder.apply(account: previousAccount)
        // Keep the domain's account key while simulating a row with an outdated DAV root.
        storedFolder.account = account.ncKitAccount
        storedFolder.serverUrl = previousAccount.davFilesUrl
        manager.addItemMetadata(storedFolder)

        var storedFile = try #require(manager.itemMetadata(ocId: file.identifier))
        storedFile.apply(account: previousAccount)
        storedFile.account = account.ncKitAccount
        storedFile.serverUrl = storedFolder.remotePath()
        manager.addItemMetadata(storedFile)
    }

    private func readFolder() async -> RemoteReadResult {
        await Enumerator.readServerUrl(
            folder.remotePath, account: account, remoteInterface: remote,
            dbManager: manager, depth: .targetAndDirectChildren, log: FileProviderLogMock()
        )
    }

    private func makeEnumerator(_ identifier: NSFileProviderItemIdentifier) throws -> Enumerator {
        try Enumerator(
            enumeratedItemIdentifier: identifier, account: account,
            remoteInterface: remote, dbManager: manager, log: FileProviderLogMock()
        )
    }
}

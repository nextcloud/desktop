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
        let initialRead = await remote.enumerate(remotePath: configuredURL.absoluteString, depth: .target, account: account)
        try #require(initialRead.error == .success)
        let metadata = try #require(initialRead.files.first).toItemMetadata()
        try #require(metadata.urlBase == account.serverUrl)
        try #require(metadata.remotePath() == parsedURL.absoluteString)

        let manager = DatabaseTestSuites.makeManager(account: account)
        let result = await Enumerator.readServerUrl(
            metadata.remotePath(),
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
        storedFolder.serverUrl = previousAccount.davFilesUrl
        manager.addItemMetadata(storedFolder)

        var storedFile = try #require(manager.itemMetadata(ocId: file.identifier))
        storedFile.apply(account: previousAccount)
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

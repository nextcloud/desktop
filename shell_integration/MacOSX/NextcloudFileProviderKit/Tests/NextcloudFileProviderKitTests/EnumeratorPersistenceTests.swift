//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import Testing
@testable import TestInterface

@Suite("Enumerator metadata persistence")
struct EnumeratorPersistenceTests {
    private let account = DatabaseTestSuites.account
    private let manager = DatabaseTestSuites.makeManager()
    private let folder: MockRemoteItem
    private let file: MockRemoteItem
    private let remote: MockRemoteInterface
    private let anchorDate = Date(timeIntervalSinceNow: -60)

    init() {
        let account = DatabaseTestSuites.account
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

    @Test func workingSetSkipsAStaleURLThatWouldReturn404() async throws {
        let previousAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://previous.example.com",
            password: account.password
        )
        try storeFolderAndFile(at: previousAccount)
        remote.enumerateErrorBySuffix = ["/folder": NKError(statusCode: 404, fallbackDescription: "File not found")]
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))

        try await observer.enumerateChanges(from: anchor)

        #expect(remote.readOperationCount == 0)
        #expect(observer.changedItems.isEmpty)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(observer.finishes.last?.anchor.rawValue == anchor.rawValue)
        #expect(manager.itemMetadata(ocId: folder.identifier)?.deleted == false)
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

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

    @Test(arguments: [Status.inUpload, .uploading, .uploadError].map(\.rawValue))
    func workingSetFileReadKeepsPendingUpload(status: Int) async throws {
        manager.setMaterializationStates([(ocId: folder.identifier, downloaded: nil, visitedDirectory: false)])
        var pending = try #require(manager.itemMetadata(ocId: file.identifier))
        pending.status = status
        pending.uploaded = false
        pending.downloaded = true
        pending.chunkUploadId = "pending-chunks"
        pending.sessionError = status == Status.uploadError.rawValue ? "upload failed" : nil
        manager.addItemMetadata(pending)

        let observer = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        try await observer.enumerateChanges(from: anchor)

        #expect(remote.readOperationCount == 1)
        #expect(manager.itemMetadata(ocId: file.identifier) == pending)
        #expect(observer.changedItems.isEmpty)
        #expect(observer.deletedItemIdentifiers.isEmpty)
    }

    @Test func fileReadReportsPinSetDuringRequest() async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        let manager = manager
        remote.enumerateHandler = {
            _ = try? manager.set(keepDownloaded: true, for: original)
        }
        file.versionIdentifier = "file-v2"

        let result = await Enumerator.readServerUrl(
            file.remotePath,
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            depth: .target,
            log: FileProviderLogMock()
        )

        let stored = try #require(manager.itemMetadata(ocId: file.identifier))
        #expect(result.error == nil)
        #expect(stored.etag == "file-v2")
        #expect(stored.keepDownloaded)
        #expect(result.metadatas == [stored])
        #expect(result.changes?.updated == [stored])
    }

    @Test func failedFileCacheWriteReturnsReconciledMetadata() async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        _ = try manager.set(keepDownloaded: true, for: original)
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(true)

        let result = await Enumerator.readServerUrl(
            file.remotePath,
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            depth: .target,
            log: FileProviderLogMock()
        )

        #expect(result.error == nil)
        let metadata = try #require(result.metadatas?.first)
        #expect(metadata.etag == "file-v2")
        #expect(metadata.keepDownloaded)
        #expect(result.changes?.updated == [metadata])
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == original.etag)
    }

    @Test(arguments: [Status.inUpload, .uploading, .uploadError].map(\.rawValue))
    func folderReadKeepsPendingChildUpload(status: Int) async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        var pending = try #require(manager.setStatusForItemMetadata(original, status: .uploading))
        pending.status = status
        pending.chunkUploadId = "pending-chunks"
        manager.addItemMetadata(pending)

        file.versionIdentifier = "file-v2"
        let result = await readFolder()

        #expect(result.error == nil)
        #expect(manager.itemMetadata(ocId: file.identifier) == pending)
        #expect(result.changes?.isEmpty == true)
    }

    @Test func singleFileReadRetriesAfterUploadFinishes() async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        let pending = try #require(manager.setStatusForItemMetadata(original, status: .uploading))
        let manager = manager
        remote.enumerateHandler = {
            // The in-flight read still returns file-v1 after the upload completes locally.
            _ = manager.recordSuccessfulUpload(
                pending,
                etag: "uploaded-etag",
                modificationDate: pending.date,
                size: pending.size
            )
        }

        let stale = await readFile()

        #expect(stale.error?.fileProviderError?.code == .cannotSynchronize)
        #expect(stale.metadatas == nil)
        #expect(stale.changes == nil)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "uploaded-etag")

        remote.enumerateHandler = nil
        file.versionIdentifier = "server-v3"
        let fresh = await readFile()

        #expect(fresh.error == nil)
        #expect(fresh.changes?.updated.first?.etag == "server-v3")
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "server-v3")
    }

    @Test func folderReadRefreshesServerCopyAfterUploadConflict() async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        _ = try #require(manager.setStatusForItemMetadata(original, status: .uploading))
        manager.recordUploadFailure(
            ocId: file.identifier,
            errorDescription: "Precondition Failed",
            staleLockToken: nil,
            serverVersionConflict: true
        )
        file.versionIdentifier = "server-after-conflict"

        let result = await readFolder()

        #expect(result.error == nil)
        #expect(result.changes?.created.isEmpty == true)
        #expect(result.changes?.updated.first?.etag == "server-after-conflict")
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "server-after-conflict")
    }

    private func readFile() async -> RemoteReadResult {
        await Enumerator.readServerUrl(
            file.remotePath,
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            depth: .target,
            log: FileProviderLogMock()
        )
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

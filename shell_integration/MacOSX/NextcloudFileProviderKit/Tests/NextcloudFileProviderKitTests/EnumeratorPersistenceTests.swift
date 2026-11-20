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
            _ = try? manager.recordSuccessfulUpload(
                pending,
                uploadedOcId: pending.ocId,
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

    enum RenameDiscovery: CaseIterable {
        case singleItem
        case folderChanges
        case folderPage
    }

    @Test(arguments: RenameDiscovery.allCases)
    func remoteRenamePreservesFailedUpload(discovery: RenameDiscovery) async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        let targetBeforeRename = Item(
            metadata: original,
            parentItemIdentifier: .init(folder.identifier),
            account: account,
            remoteInterface: remote,
            dbManager: manager
        )
        var pending = try #require(manager.setStatusForItemMetadata(original, status: .uploading))
        pending.chunkUploadId = "pending-chunks"
        manager.addItemMetadata(pending)
        manager.recordUploadFailure(
            ocId: file.identifier,
            errorDescription: "Not Found",
            staleLockToken: nil,
            serverVersionConflict: false
        )
        file.name = "renamed.txt"
        file.remotePath = folder.remotePath + "/" + file.name
        file.versionIdentifier = "server-etag"

        let result: RemoteReadResult
        switch discovery {
        case .singleItem:
            result = await readFile()
        case .folderChanges:
            result = await readFolder()
        case .folderPage:
            result = await Enumerator.readServerUrl(
                folder.remotePath,
                pageSettings: (page: nil, index: 0, size: 10),
                account: account,
                remoteInterface: remote,
                dbManager: manager,
                log: FileProviderLogMock()
            )
        }

        #expect(result.error == nil)
        let stored = try #require(manager.itemMetadata(ocId: file.identifier))
        #expect(stored.remotePath() == file.remotePath)
        #expect(stored.status == Status.uploadError.rawValue)
        #expect(stored.sessionError == "Not Found")
        #expect(stored.chunkUploadId == pending.chunkUploadId)
        #expect(stored.etag == original.etag)
        #expect(!stored.uploaded)
        if discovery == .folderPage {
            #expect(result.metadatas?.first { $0.ocId == file.identifier } == stored)
        } else {
            #expect(result.changes?.created.isEmpty == true)
            #expect(result.changes?.updated == [stored])
        }

        let currentItem = Item(
            metadata: stored,
            parentItemIdentifier: .init(folder.identifier),
            account: account,
            remoteInterface: remote,
            dbManager: manager
        )
        let edit = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let contents = Data("Retry after the server rename".utf8)
        try contents.write(to: edit)
        defer { try? FileManager.default.removeItem(at: edit) }

        let (modified, error) = await currentItem.modify(
            itemTarget: targetBeforeRename,
            changedFields: [.contents],
            contents: edit,
            dbManager: manager
        )

        #expect(error == nil)
        #expect(modified?.metadata.remotePath() == file.remotePath)
        #expect(file.data == contents)
        #expect(folder.children.count == 1)
    }

    @Test(arguments: [false, true])
    func olderFolderListingKeepsRediscoveredUploadPath(paged: Bool) async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        _ = try #require(manager.setStatusForItemMetadata(original, status: .uploading))
        manager.recordUploadFailure(
            ocId: file.identifier,
            errorDescription: "Not Found",
            staleLockToken: nil,
            serverVersionConflict: false
        )
        var renamed = original
        renamed.apply(fileName: "renamed.txt")
        let newerMetadata = renamed
        let manager = manager
        remote.enumerateHandler = {
            // A newer single-item response commits the rename while the folder read is in flight.
            do {
                _ = try await manager.reconcileItemMetadataFromRemoteRead(
                    newerMetadata,
                    readStartedAt: Date(),
                    isCancelled: { false }
                )
            } catch {
                Issue.record(error)
            }
        }

        let result = await Enumerator.readServerUrl(
            folder.remotePath,
            pageSettings: paged ? (page: nil, index: 0, size: 10) : nil,
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            log: FileProviderLogMock()
        )

        #expect(result.error == nil)
        let stored = try #require(manager.itemMetadata(ocId: file.identifier))
        #expect(stored.remotePath() == newerMetadata.remotePath())
        #expect(stored.status == Status.uploadError.rawValue)
        if paged {
            #expect(result.metadatas?.first { $0.ocId == original.ocId } == stored)
        } else {
            #expect(result.changes?.isEmpty == true)
        }
    }

    @Test func completedUploadUpdatesTheExistingRow() async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        var pinned = original
        pinned.keepDownloaded = true
        manager.addItemMetadata(pinned)
        file.versionIdentifier = "file-v2"
        let contents = Data("New uploaded content".utf8)
        let modifiedAt = Date(timeIntervalSince1970: 1_700_000_000.125)
        try await manager.rejectMetadataWriteForTesting(.insert, size: Int64(contents.count))
        var reads: [EnumerateDepth] = []
        remote.enumerateCallHandler = { [file] path, depth, _, _, _, _, _, _ in
            if path == file.remotePath {
                reads.append(depth)
            }
        }

        let (reported, error) = try await uploadEdit(metadata: pinned, contents: contents, modifiedAt: modifiedAt)

        #expect(error == nil)
        #expect(reads.isEmpty)
        #expect(remote.uploadOperationCount == 1)
        #expect(file.data == contents)
        let stored = try #require(manager.itemMetadata(ocId: file.identifier))
        #expect(stored.etag == "file-v2")
        #expect(stored.fileProviderContentVersion == "file-v2")
        #expect(stored.size == Int64(contents.count))
        #expect(stored.date == modifiedAt)
        #expect(stored.syncTime > original.syncTime)
        #expect(stored.keepDownloaded)
        #expect(stored.uploaded)
        #expect(stored.downloaded)
        #expect(stored.status == Status.normal.rawValue)
        #expect(reported?.metadata == stored)
    }

    @Test(arguments: [nil, "", "W/\"file-v2\""] as [String?])
    func uploadWithoutUsableETagLeavesThePreviousMetadataIntact(responseETag: String?) async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        let contents = Data("Uploaded without a usable response ETag".utf8)
        remote.omitUploadETag = responseETag == nil
        file.versionIdentifier = responseETag ?? "file-v2"
        var readUploadedFile = false
        remote.enumerateCallHandler = { [file] path, _, _, _, _, _, _, _ in
            if path == file.remotePath {
                readUploadedFile = true
            }
        }

        let (reported, error) = try await uploadEdit(metadata: original, contents: contents, modifiedAt: Date())

        #expect(reported == nil)
        #expect(error as? NSFileProviderError == NSFileProviderError(.cannotSynchronize))
        #expect(!readUploadedFile)
        #expect(remote.uploadOperationCount == 1)
        #expect(file.data == contents)
        let stored = try #require(manager.itemMetadata(ocId: file.identifier))
        #expect(stored.etag == original.etag)
        #expect(stored.fileProviderContentVersion == original.fileProviderContentVersion)
        #expect(stored.size == original.size)
        #expect(stored.date == original.date)
        #expect(stored.syncTime == original.syncTime)
        #expect(stored.status == Status.uploadError.rawValue)
        #expect(stored.hasPendingUpload)
    }

    @Test func failedCompletionUpdateReturnsAnErrorWithoutPublishingPartOfTheUpload() async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        let contents = Data("On the server despite a failed database write".utf8)
        file.versionIdentifier = "file-v2"
        try await manager.rejectMetadataWriteForTesting(.update, size: Int64(contents.count))

        let (reported, error) = try await uploadEdit(metadata: original, contents: contents, modifiedAt: Date())

        #expect(reported == nil)
        #expect(error as? NSFileProviderError == NSFileProviderError(.cannotSynchronize))
        #expect(remote.uploadOperationCount == 1)
        #expect(file.data == contents)
        let stored = try #require(manager.itemMetadata(ocId: file.identifier))
        #expect(stored.etag == original.etag)
        #expect(stored.size == original.size)
        #expect(stored.date == original.date)
        #expect(stored.syncTime == original.syncTime)
        #expect(stored.hasPendingUpload)
    }

    @Test(arguments: [true, false])
    func uploadCompletesAfterParentRenameOnlyWithMatchingIdentity(matchesIdentity: Bool) async throws {
        var original = try #require(manager.itemMetadata(ocId: file.identifier))
        original.fileId = "42"
        manager.addItemMetadata(original)
        let originalOcId = original.ocId
        let oldParentPath = folder.remotePath
        let renamedParentPath = account.davFilesUrl + "/renamed-folder"
        let contents = Data("Uploaded before parent rename".utf8)
        file.versionIdentifier = "file-v2"
        if !matchesIdentity {
            file.identifier = "replacement-file"
        }
        remote.uploadCompletionHandler = { [weak remote, manager, account] in
            guard let remote else { return }
            let (_, _, moveError) = await remote.move(
                remotePathSource: oldParentPath,
                remotePathDestination: renamedParentPath,
                overwrite: false,
                account: account,
                options: .init(),
                taskHandler: { _ in }
            )
            #expect(moveError == .success)
            let read = await Enumerator.readServerUrl(
                account.davFilesUrl,
                account: account,
                remoteInterface: remote,
                dbManager: manager,
                log: FileProviderLogMock()
            )
            #expect(read.error == nil)
            #expect(manager.itemMetadata(ocId: originalOcId)?.serverUrl == renamedParentPath)
            #expect(manager.itemMetadata(ocId: originalOcId)?.status == Status.uploading.rawValue)
        }

        let (reported, error) = try await uploadEdit(metadata: original, contents: contents, modifiedAt: Date())

        #expect(remote.uploadOperationCount == 1)
        #expect(file.data == contents)
        let stored = try #require(manager.itemMetadata(ocId: original.ocId))
        #expect(stored.serverUrl == renamedParentPath)
        if matchesIdentity {
            #expect(error == nil)
            #expect(reported?.metadata == stored)
            #expect(reported?.parentItemIdentifier == NSFileProviderItemIdentifier(folder.identifier))
            #expect(stored.etag == "file-v2")
            #expect(stored.size == Int64(contents.count))
            #expect(stored.status == Status.normal.rawValue)
            #expect(stored.uploaded)
        } else {
            #expect(error as? NSFileProviderError == NSFileProviderError(.cannotSynchronize))
            #expect(reported == nil)
            #expect(stored.etag == original.etag)
            #expect(stored.size == original.size)
            #expect(stored.hasPendingUpload)
        }
    }

    private func uploadEdit(
        metadata: SendableItemMetadata,
        contents: Data,
        modifiedAt: Date
    ) async throws -> (Item?, Error?) {
        let parent = NSFileProviderItemIdentifier(folder.identifier)
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: parent,
            account: account,
            remoteInterface: remote,
            dbManager: manager
        )
        var edited = metadata
        edited.date = modifiedAt
        let target = Item(
            metadata: edited,
            parentItemIdentifier: parent,
            account: account,
            remoteInterface: remote,
            dbManager: manager
        )
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: url) }
        try contents.write(to: url)
        return await item.modify(
            itemTarget: target,
            baseVersion: item.itemVersion,
            changedFields: [.contents, .contentModificationDate],
            contents: url,
            dbManager: manager
        )
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

    @Test(arguments: [false, true])
    func listingDefersDifferentItemAtPendingUploadPath(paginated: Bool) async throws {
        let original = try #require(manager.itemMetadata(ocId: file.identifier))
        var pending = try #require(manager.setStatusForItemMetadata(original, status: .uploading))
        pending.downloaded = true
        pending.chunkUploadId = "pending-chunks"
        manager.addItemMetadata(pending)

        let moved = await remote.move(
            remotePathSource: file.remotePath,
            remotePathDestination: folder.remotePath + "/renamed.txt",
            account: account
        )
        try #require(moved.error == .success)

        let replacement = MockRemoteItem(
            identifier: "replacement",
            name: pending.fileName,
            remotePath: pending.remotePath(),
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        replacement.parent = folder
        folder.children.append(replacement)
        remote.pagination = paginated

        let pageSize = 2
        let result = await Enumerator.readServerUrl(
            folder.remotePath,
            pageSettings: paginated ? (nil, 0, pageSize) : nil,
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            depth: .targetAndDirectChildren,
            log: FileProviderLogMock()
        )
        #expect(result.error == nil)
        var listed = try #require(result.metadatas)
        if paginated {
            let continuation = try #require(result.nextPage)
            let token = try #require(continuation.token)
            let nextPage = NSFileProviderPage(Data(token.utf8))
            let next = await Enumerator.readServerUrl(
                folder.remotePath,
                pageSettings: (nextPage, continuation.index, pageSize),
                account: account,
                remoteInterface: remote,
                dbManager: manager,
                depth: .targetAndDirectChildren,
                log: FileProviderLogMock()
            )
            #expect(next.error == nil)
            #expect(next.nextPage == nil)
            listed += try #require(next.metadatas)
        } else {
            let changes = try #require(result.changes)
            #expect(!changes.createdAndUpdated.contains { $0.ocId == replacement.identifier })
        }
        #expect(!listed.contains { $0.ocId == replacement.identifier })
        #expect(manager.itemMetadata(ocId: replacement.identifier) == nil)
        #expect(manager.itemMetadata(ocId: pending.ocId) == pending)

        let targetRead = await Enumerator.readServerUrl(
            pending.remotePath(),
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            depth: .target,
            log: FileProviderLogMock()
        )
        #expect(targetRead.error == nil)
        #expect(targetRead.metadatas?.map(\.ocId) == [pending.ocId])
        #expect(targetRead.changes?.createdAndUpdated.isEmpty == true)
        #expect(manager.itemMetadata(ocId: replacement.identifier) == nil)

        manager.addItemMetadata(file.toItemMetadata(account: account))
        let refreshed = await readFolder()
        #expect(refreshed.error == nil)
        let changes = try #require(refreshed.changes)
        #expect(changes.created.map(\.ocId) == [replacement.identifier])
        #expect(manager.itemMetadata(ocId: replacement.identifier)?.remotePath() == pending.remotePath())
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

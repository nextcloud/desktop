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

    @Test(arguments: [false, true])
    func pagedPersistenceReturnsNilWhenWriteFails(existingItem: Bool) throws {
        var metadata = file.toItemMetadata(account: account)
        metadata.etag = "file-v2"
        if !existingItem {
            metadata.ocId = "new-file"
            metadata.fileName = "new-file.txt"
        }
        try manager.setItemMetadataWriteFailureForTesting(true)

        #expect(manager.persistPaginatedMetadata(target: nil, children: [metadata]) == nil)

        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v1")
        #expect(manager.itemMetadata(ocId: "new-file") == nil)
    }

    @Test func pagedTargetWriteFailureStopsBeforePersistingChildren() throws {
        folder.versionIdentifier = "folder-v2"
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(true, ocId: folder.identifier)

        let result = Enumerator.handlePagedReadResults(
            files: [folder.toNKFile(), file.toNKFile()], pageIndex: 0,
            dbManager: manager, log: FileProviderLogMock()
        )

        #expect(result.error == .metadataDatabaseWriteFailed)
        #expect(result.metadatas == nil)
        #expect(manager.itemMetadata(ocId: folder.identifier)?.etag == "folder-v1")
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v1")
    }

    @Test(arguments: [0, 1])
    func pagedChildWriteFailureRollsBackTheWholePageAndAllowsRetry(pageIndex: Int) throws {
        var storedFolder = folder.toNKFile().toItemMetadata()
        storedFolder.visitedDirectory = pageIndex > 0
        storedFolder.keepDownloaded = true
        manager.addItemMetadata(storedFolder)
        var stored = file.toNKFile().toItemMetadata()
        stored.keepDownloaded = true
        stored.downloaded = true
        manager.addItemMetadata(stored)
        folder.versionIdentifier = "folder-v2"
        file.versionIdentifier = "file-v2"
        var newFile = file.toNKFile()
        newFile.ocId = "new-file"
        newFile.fileId = "new-file"
        newFile.fileName = "new-file.txt"
        var oldFile = newFile.toItemMetadata()
        oldFile.ocId = "old-file"
        oldFile.fileId = "old-file"
        oldFile.downloaded = true
        oldFile.keepDownloaded = true
        manager.addItemMetadata(oldFile)
        var failingFile = newFile
        failingFile.ocId = "failing-file"
        failingFile.fileId = "failing-file"
        failingFile.fileName = "failing-file.txt"
        let files = (pageIndex == 0 ? [folder.toNKFile()] : []) + [file.toNKFile(), newFile, failingFile]
        try manager.setItemMetadataWriteFailureForTesting(true, ocId: failingFile.ocId)

        let failed = Enumerator.handlePagedReadResults(
            files: files, pageIndex: pageIndex, dbManager: manager, log: FileProviderLogMock()
        )

        #expect(failed.error == .metadataDatabaseWriteFailed)
        #expect(failed.metadatas == nil)
        #expect(manager.itemMetadata(ocId: newFile.ocId) == nil)
        #expect(manager.itemMetadata(ocId: oldFile.ocId)?.deleted == false)
        #expect(manager.itemMetadata(ocId: failingFile.ocId) == nil)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v1")
        #expect(manager.itemMetadata(ocId: file.identifier)?.keepDownloaded == true)
        #expect(manager.itemMetadata(ocId: file.identifier)?.downloaded == true)
        #expect(manager.itemMetadata(ocId: folder.identifier)?.etag == "folder-v1")
        #expect(manager.itemMetadata(ocId: folder.identifier)?.visitedDirectory == storedFolder.visitedDirectory)

        try manager.setItemMetadataWriteFailureForTesting(false)
        let recovered = Enumerator.handlePagedReadResults(
            files: files, pageIndex: pageIndex, dbManager: manager, log: FileProviderLogMock()
        )

        #expect(recovered.error == nil)
        #expect(recovered.metadatas?.map(\.ocId) == [file.identifier, newFile.ocId, failingFile.ocId])
        let reported = try #require(recovered.metadatas?.first { $0.ocId == file.identifier })
        #expect(reported.etag == "file-v2")
        #expect(reported.keepDownloaded)
        #expect(reported.downloaded)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == reported.etag)
        #expect(manager.itemMetadata(ocId: file.identifier)?.keepDownloaded == reported.keepDownloaded)
        #expect(manager.itemMetadata(ocId: file.identifier)?.downloaded == reported.downloaded)
        #expect(manager.itemMetadata(ocId: newFile.ocId)?.keepDownloaded == true)
        #expect(manager.itemMetadata(ocId: newFile.ocId)?.downloaded == true)
        #expect(manager.itemMetadata(ocId: oldFile.ocId)?.deleted == true)
        #expect(manager.itemMetadata(ocId: failingFile.ocId)?.keepDownloaded == true)
        #expect(manager.itemMetadata(ocId: folder.identifier)?.etag == (pageIndex == 0 ? "folder-v2" : "folder-v1"))
        #expect(manager.itemMetadata(ocId: folder.identifier)?.visitedDirectory == true)
    }

    @Test func pagedRemoteReadPropagatesPersistenceFailure() async throws {
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(true, ocId: file.identifier)

        let result = await Enumerator.readServerUrl(
            folder.remotePath, pageSettings: (page: nil, index: 0, size: 100),
            account: account, remoteInterface: remote, dbManager: manager,
            depth: .targetAndDirectChildren, log: FileProviderLogMock()
        )

        #expect(remote.readOperationCount == 1)
        #expect(result.error == .metadataDatabaseWriteFailed)
        #expect(result.error?.fileProviderError?.code == .cannotSynchronize)
        #expect(result.metadatas == nil)
        #expect(result.changes == nil)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v1")
    }

    @Test func failedPageDoesNotHideLockChangesFromTheWorkingSet() async throws {
        let account = Account(user: UUID().uuidString, id: "user", serverUrl: "https://mock.nc.com", password: "password")
        let manager = DatabaseTestSuites.makeManager(account: account)
        let root = MockRemoteItem.rootItem(account: account)
        let folder = MockRemoteItem(
            identifier: "folder", versionIdentifier: "folder-v1", name: "folder",
            remotePath: account.davFilesUrl + "/folder", directory: true,
            account: account.ncKitAccount, username: account.username,
            userId: account.id, serverUrl: account.serverUrl
        )
        let file = MockRemoteItem(
            identifier: "file", versionIdentifier: "file-v1", name: "file.txt",
            remotePath: folder.remotePath + "/file.txt", account: account.ncKitAccount,
            username: account.username, userId: account.id, serverUrl: account.serverUrl
        )
        let laterFile = MockRemoteItem(
            identifier: "later-file", versionIdentifier: "later-v1", name: "later.txt",
            remotePath: folder.remotePath + "/later.txt", account: account.ncKitAccount,
            username: account.username, userId: account.id, serverUrl: account.serverUrl
        )
        folder.parent = root
        root.children = [folder]
        file.parent = folder
        laterFile.parent = folder
        folder.children = [file, laterFile]
        let baselineDate = Date(timeIntervalSince1970: anchorDate.timeIntervalSince1970.rounded(.down) - 60)
        root.modificationDate = baselineDate
        folder.modificationDate = baselineDate
        file.modificationDate = baselineDate
        laterFile.modificationDate = baselineDate
        for directory in [root, folder] {
            var metadata = directory.toNKFile().toItemMetadata()
            metadata.visitedDirectory = true
            metadata.syncTime = baselineDate
            manager.addItemMetadata(metadata)
        }
        for child in folder.children {
            var metadata = child.toNKFile().toItemMetadata()
            // A downloaded sibling keeps the folder eligible for a scan even with an unchanged ETag.
            metadata.downloaded = child.identifier == laterFile.identifier
            metadata.syncTime = baselineDate
            manager.addItemMetadata(metadata)
        }
        file.locked = true
        file.lockOwner = "other-user"
        file.lockTimeOut = anchorDate.addingTimeInterval(3600)
        laterFile.locked = true
        laterFile.lockOwner = file.lockOwner
        laterFile.lockTimeOut = file.lockTimeOut
        let remote = MockRemoteInterface(account: account, rootItem: root, pagination: true)
        remote.capabilities = remote.capabilities.replacingOccurrences(of: "\"major\": 28", with: "\"major\": 31")
        #expect(remote.directMockCapabilities()?.major == 31)
        let pageSize = folder.children.count + 1
        remote.enumerateCallHandler = { path, depth, _, _, _, _, options, _ in
            #expect(path == folder.remotePath)
            #expect(depth == .targetAndDirectChildren)
            #expect(options.paginate)
            #expect(options.paginateOffset == 0)
            #expect(options.paginateCount == pageSize)
            #expect(options.paginateToken == nil)
        }
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .init(folder.identifier), account: account, remoteInterface: remote,
            dbManager: manager, pageSize: pageSize, log: FileProviderLogMock()
        )
        try manager.setItemMetadataWriteFailureForTesting(true, ocId: laterFile.identifier)
        let page = NSFileProviderPage.initialPageSortedByName as NSFileProviderPage
        let failed = MockEnumerationObserver(enumerator: enumerator)

        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await failed.enumerateItemsPage(page: page)
        }

        #expect(remote.readOperationCount == 1)
        #expect(failed.items.isEmpty)
        #expect(manager.itemMetadata(ocId: file.identifier)?.lock == false)
        #expect(manager.itemMetadata(ocId: file.identifier)?.downloaded == false)
        #expect(manager.itemMetadata(ocId: file.identifier)?.syncTime == baselineDate)
        #expect(manager.itemMetadata(ocId: folder.identifier)?.etag == "folder-v1")
        #expect(manager.itemMetadata(ocId: folder.identifier)?.syncTime == baselineDate)
        #expect(manager.itemMetadata(ocId: laterFile.identifier)?.etag == "later-v1")
        #expect(manager.itemMetadata(ocId: laterFile.identifier)?.lock == false)

        try manager.setItemMetadataWriteFailureForTesting(false)
        remote.enumerateCallHandler = nil
        // Change enumeration must still discover the lock before the failed item page is retried.
        let workingSet = try Enumerator(
            enumeratedItemIdentifier: .workingSet, account: account, remoteInterface: remote,
            dbManager: manager, log: FileProviderLogMock()
        )
        let changes = MockChangeObserver(enumerator: workingSet)
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        try await changes.enumerateChanges(from: anchor)

        #expect(changes.error == nil)
        #expect(changes.deletedItemIdentifiers.isEmpty)
        let changedFile = try #require(changes.changedItems.first { $0.itemIdentifier.rawValue == file.identifier } as? Item)
        #expect(changedFile.metadata.lock)
        #expect(changedFile.metadata.etag == "file-v1")
        #expect(!changedFile.metadata.downloaded)
        #expect(changedFile.metadata.lockOwner == file.lockOwner)
        #expect(!changedFile.capabilities.contains(.allowsWriting))
        #expect(manager.itemMetadata(ocId: file.identifier)?.lock == true)

        let recovered = MockEnumerationObserver(enumerator: enumerator)
        try await recovered.enumerateItemsPage(page: page)

        #expect(recovered.error == nil)
        #expect(Set(recovered.items.map(\.itemIdentifier)) == Set([file, laterFile].map { NSFileProviderItemIdentifier($0.identifier) }))
        let reportedFile = try #require(recovered.items.first { $0.itemIdentifier.rawValue == file.identifier } as? Item)
        #expect(reportedFile.metadata.lock)
        #expect(!reportedFile.capabilities.contains(.allowsWriting))
        #expect(recovered.page == nil)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v1")
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

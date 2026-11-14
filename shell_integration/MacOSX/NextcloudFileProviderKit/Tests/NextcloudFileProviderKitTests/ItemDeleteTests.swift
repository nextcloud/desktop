//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
@testable import NextcloudKit
import os
import Testing
@testable import TestInterface
import UniformTypeIdentifiers
import XCTest

final class ItemDeleteTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )
    lazy var rootItem = MockRemoteItem.rootItem(account: Self.account)
    lazy var rootTrashItem = MockRemoteItem.rootTrashItem(account: Self.account)
    static let dbManager = try! FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        setUpDatabase(Self.dbManager)
    }

    override func tearDown() {
        rootItem.children = []
        rootTrashItem.children = []
        super.tearDown()
    }

    private func makeLockFileDeletionScenario(
        targetFileName: String,
        remoteInterface: MockRemoteInterface
    ) -> (
        lockItem: Item,
        targetRemote: MockRemoteItem,
        targetMetadata: SendableItemMetadata,
        lockFileMetadata: SendableItemMetadata
    ) {
        let folderName = "folder-\(targetFileName)"
        let folderRemote = MockRemoteItem(
            identifier: "\(folderName)-id",
            versionIdentifier: "1",
            name: folderName,
            remotePath: Self.account.davFilesUrl + "/" + folderName,
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let targetRemote = MockRemoteItem(
            identifier: "\(folderName)/\(targetFileName)",
            versionIdentifier: "1",
            name: targetFileName,
            remotePath: folderRemote.remotePath + "/" + targetFileName,
            data: Data("test data".utf8),
            locked: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        folderRemote.children = [targetRemote]
        folderRemote.parent = rootItem
        rootItem.children = [folderRemote]

        var folderMetadata = SendableItemMetadata(
            ocId: folderRemote.identifier,
            fileName: folderName,
            account: Self.account
        )
        folderMetadata.directory = true
        Self.dbManager.addItemMetadata(folderMetadata)

        var targetMetadata = SendableItemMetadata(
            ocId: targetRemote.identifier,
            fileName: targetFileName,
            account: Self.account
        )
        targetMetadata.serverUrl = folderRemote.remotePath
        targetMetadata.lock = true
        targetMetadata.lockOwner = Self.account.id
        targetMetadata.lockOwnerDisplayName = "Test User"
        targetMetadata.lockOwnerEditor = "desktop"
        targetMetadata.lockOwnerType = NKLockType.token.rawValue
        targetMetadata.lockTime = Date(timeIntervalSince1970: 1)
        targetMetadata.lockTimeOut = Date(timeIntervalSince1970: 2)
        targetMetadata.lockToken = "opaquelocktoken:test-token"
        Self.dbManager.addItemMetadata(targetMetadata)

        var lockFileMetadata = SendableItemMetadata(
            ocId: "\(folderName)-lock-id",
            fileName: ".~lock.\(targetFileName)#",
            account: Self.account
        )
        lockFileMetadata.serverUrl = folderRemote.remotePath
        Self.dbManager.addItemMetadata(lockFileMetadata)

        let lockItem = Item(
            metadata: lockFileMetadata,
            parentItemIdentifier: .init(folderMetadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        return (lockItem, targetRemote, targetMetadata, lockFileMetadata)
    }

    func testDeleteFile() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let itemIdentifier = "file"
        let remoteItem = MockRemoteItem(
            identifier: itemIdentifier,
            name: "file",
            remotePath: Self.account.davFilesUrl + "/file",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteItem.parent = rootItem
        rootItem.children = [remoteItem]

        XCTAssertFalse(rootItem.children.isEmpty)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: itemIdentifier))
        remoteItem.versionIdentifier = "current-etag"

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await item.delete(dbManager: Self.dbManager)
        XCTAssertNil(error)
        XCTAssertTrue(rootItem.children.isEmpty)
        XCTAssertEqual(remoteInterface.lastDeleteIfMatchHeader, "\"current-etag\"")

        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: itemIdentifier)?.deleted, true)
    }

    func testDeleteRefusesAReplacementWithTheSameETag() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let original = MockRemoteItem(
            identifier: "original-report",
            versionIdentifier: "same-etag",
            name: "report.txt",
            remotePath: Self.account.davFilesUrl + "/report.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        original.parent = rootItem
        let cached = original.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(cached)

        let replacement = MockRemoteItem(
            identifier: "replacement-report",
            versionIdentifier: "same-etag",
            name: original.name,
            remotePath: original.remotePath,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        replacement.parent = rootItem
        original.name = "moved-report.txt"
        original.remotePath = Self.account.davFilesUrl + "/moved-report.txt"
        rootItem.children = [original, replacement]

        let item = Item(
            metadata: cached,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertNotNil(error)
        XCTAssertNil(remoteInterface.lastDeleteRemotePath)
        XCTAssertEqual(rootItem.children.map(\.identifier), [original.identifier, replacement.identifier])
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: cached.ocId)?.deleted, false)
    }

    func testDeleteFindsServerCopyAfterFailedEditWithoutAnETag() async throws {
        let remote = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteItem = MockRemoteItem(
            identifier: "edited-report",
            versionIdentifier: "server-etag",
            name: "report.txt",
            remotePath: Self.account.davFilesUrl + "/report.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteItem.parent = rootItem
        rootItem.children = [remoteItem]
        remote.uploadError = NKError(statusCode: 503, fallbackDescription: "Unavailable")

        // The previous upload response did not supply an ETag.
        var metadata = remoteItem.toItemMetadata(account: Self.account)
        metadata.etag = ""
        metadata.downloaded = true
        Self.dbManager.addItemMetadata(metadata)
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remote,
            dbManager: Self.dbManager
        )
        let editURL = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data("Updated report".utf8).write(to: editURL)
        defer { try? FileManager.default.removeItem(at: editURL) }
        let (_, editError) = await item.modify(
            itemTarget: item,
            changedFields: [.contents],
            contents: editURL,
            dbManager: Self.dbManager
        )
        XCTAssertNotNil(editError)
        let failedEdit = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: metadata.ocId))
        XCTAssertEqual(failedEdit.status, Status.uploadError.rawValue)
        XCTAssertFalse(failedEdit.uploaded)
        XCTAssertTrue(failedEdit.etag.isEmpty)

        let itemAfterEdit = Item(
            metadata: failedEdit,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remote,
            dbManager: Self.dbManager
        )
        let deleteError = await itemAfterEdit.delete(dbManager: Self.dbManager)

        XCTAssertNil(deleteError)
        XCTAssertEqual(remote.lastDeleteRemotePath, metadata.remotePath())
        XCTAssertEqual(remote.lastDeleteIfMatchHeader, "\"server-etag\"")
        XCTAssertTrue(rootItem.children.isEmpty)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: metadata.ocId)?.deleted, true)
    }

    func testDeleteTreatsMissingFileAsSuccess() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        remoteInterface.deleteError = NKError(statusCode: 404, fallbackDescription: "Not Found")

        let remoteItem = MockRemoteItem(
            identifier: "already-deleted-file",
            name: "already-deleted.txt",
            remotePath: Self.account.davFilesUrl + "/already-deleted.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteItem.parent = rootItem
        rootItem.children = [remoteItem]
        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertNil(error)
        XCTAssertEqual(remoteInterface.lastDeleteRemotePath, Self.account.davFilesUrl + "/already-deleted.txt")
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)?.deleted, true)
    }

    func testDeleteTreatsMissingFolderAsSuccessAndMarksDescendantsDeleted() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)

        let remoteFolder = MockRemoteItem(
            identifier: "already-deleted-folder",
            name: "already-deleted",
            remotePath: Self.account.davFilesUrl + "/already-deleted",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteItem = MockRemoteItem(
            identifier: "already-deleted-child",
            name: "child.txt",
            remotePath: Self.account.davFilesUrl + "/already-deleted/child.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteFolder.children = [remoteItem]
        remoteItem.parent = remoteFolder

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemMetadata)

        let folder = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await folder.delete(dbManager: Self.dbManager)

        XCTAssertNil(error)
        XCTAssertNil(remoteInterface.lastDeleteRemotePath)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: folderMetadata.ocId)?.deleted, true)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)?.deleted, true)
    }

    func testTrashPurgeUsesResolvedFilename() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteTrashItem = MockRemoteItem(
            identifier: "trashed-file",
            name: "file.d1234567890",
            remotePath: Self.account.trashUrl + "/file.d1234567890",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl,
            trashbinOriginalLocation: "file"
        )
        remoteTrashItem.parent = rootTrashItem
        rootTrashItem.children = [remoteTrashItem]

        var itemMetadata = remoteTrashItem.toItemMetadata(account: Self.account)
        itemMetadata.fileName = "file"
        itemMetadata.fileNameView = "file"
        itemMetadata.name = "file"
        itemMetadata.serverUrl = Self.account.trashUrl
        Self.dbManager.addItemMetadata(itemMetadata)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertNil(error)
        XCTAssertEqual(remoteInterface.lastDeleteRemotePath, Self.account.trashUrl + "/file.d1234567890")
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)?.deleted, true)
    }

    func testTrashPurgeLeavesEntryUnresolvedWhenListingReturns404() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        remoteInterface.trashListingError = NKError(statusCode: 404, fallbackDescription: "Not Found")

        let replacement = MockRemoteItem(
            identifier: "replacement-trash-file",
            name: "file",
            remotePath: Self.account.trashUrl + "/file",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        replacement.parent = rootTrashItem
        rootTrashItem.children = [replacement]

        var itemMetadata = SendableItemMetadata(
            ocId: "missing-trash-file",
            fileName: "file",
            account: Self.account
        )
        itemMetadata.serverUrl = Self.account.trashUrl
        itemMetadata.fileId = itemMetadata.ocId
        Self.dbManager.addItemMetadata(itemMetadata)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertEqual((error as? CocoaError)?.code, .fileReadUnknown)
        XCTAssertNil(remoteInterface.lastDeleteRemotePath)
        XCTAssertEqual(rootTrashItem.children.map(\.identifier), [replacement.identifier])
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId))
        XCTAssertNotEqual(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)?.deleted, true)
    }

    func testTrashPurgeLeavesEntryUnresolvedWhenMatchingIDHasDifferentType() async {
        let remote = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteFolder = MockRemoteItem(
            identifier: "trash-id",
            name: "Report.d1234567890",
            remotePath: Self.account.trashUrl + "/Report.d1234567890",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteFolder.parent = rootTrashItem
        rootTrashItem.children = [remoteFolder]

        var metadata = SendableItemMetadata(ocId: remoteFolder.identifier, fileName: "Report", account: Self.account)
        metadata.serverUrl = Self.account.trashUrl
        metadata.fileId = metadata.ocId
        Self.dbManager.addItemMetadata(metadata)
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remote,
            dbManager: Self.dbManager
        )
        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertEqual((error as? CocoaError)?.code, .fileReadUnknown)
        XCTAssertNil(remote.lastDeleteRemotePath)
        XCTAssertEqual(rootTrashItem.children.map(\.identifier), [remoteFolder.identifier])
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: metadata.ocId)?.deleted, false)
    }

    func testDeleteUsesFilesRootRelativeIgnorePath() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteFolder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteItem = MockRemoteItem(
            identifier: "ignored-item",
            name: "item.blend1",
            remotePath: Self.account.davFilesUrl + "/folder/item.blend1",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItem]
        remoteItem.parent = remoteFolder

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let ignoredMatcher = IgnoredFilesMatcher(ignoreList: ["folder/*.blend1"], log: FileProviderLogMock())

        let error = await item.delete(ignoredFiles: ignoredMatcher, dbManager: Self.dbManager)

        XCTAssertNil(error)
        XCTAssertTrue(remoteFolder.children.contains { $0.identifier == remoteItem.identifier })
        XCTAssertNil(remoteInterface.lastDeleteRemotePath)
        XCTAssertTrue(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)?.deleted == true)
    }

    func testDeleteFileDiscardsIncompleteChunkUpload() async throws {
        let remoteInterface = MockRemoteInterface(
            account: Self.account,
            rootItem: rootItem,
            rootTrashItem: rootTrashItem
        )
        let itemIdentifier = "file-with-incomplete-upload"
        let remoteItem = MockRemoteItem(
            identifier: itemIdentifier,
            name: "file.txt",
            remotePath: Self.account.davFilesUrl + "/file.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteItem.parent = rootItem
        rootItem.children = [remoteItem]

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let chunkUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: 8,
            modificationDate: Date(timeIntervalSince1970: 1_700_000_000)
        )
        let chunksDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("deleted-item-chunks-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: chunksDirectory, withIntermediateDirectories: true)
        try Data([1]).write(to: chunksDirectory.appendingPathComponent("2"))
        remoteInterface.chunkUploadDirectories[chunkUploadId] = chunksDirectory
        defer { try? FileManager.default.removeItem(at: chunksDirectory) }

        Self.dbManager.addRemoteFileChunks([
            RemoteFileChunk(
                fileName: "2",
                size: 3,
                remoteChunkStoreFolderName: chunkUploadId
            )
        ])

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertNil(error)
        XCTAssertFalse(FileManager.default.fileExists(atPath: chunksDirectory.path))
        XCTAssertEqual(
            Self.dbManager.remoteFileChunks(uploadId: chunkUploadId)?.count,
            0
        )
    }

    func testDeleteUnexcludedBundlePropagatesToServer() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteBundle = MockRemoteItem(
            identifier: "bundle-id",
            name: "ExplicitlyDeleted.key",
            remotePath: Self.account.davFilesUrl + "/ExplicitlyDeleted.key",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteBundle.parent = rootItem
        rootItem.children = [remoteBundle]

        var metadata = remoteBundle.toItemMetadata(account: Self.account)
        metadata.contentType = UTType.bundle.identifier
        Self.dbManager.addItemMetadata(metadata)

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertNil(error)
        XCTAssertTrue(rootItem.children.isEmpty)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: metadata.ocId)?.deleted, true)
    }

    func testDeleteExcludedBundleDoesNotPropagateToServer() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteBundle = MockRemoteItem(
            identifier: "excluded-bundle-id",
            name: "Excluded.key",
            remotePath: Self.account.davFilesUrl + "/Excluded.key",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteBundle.parent = rootItem
        rootItem.children = [remoteBundle]

        var metadata = remoteBundle.toNKFile().toItemMetadata()
        metadata.contentType = UTType.bundle.identifier
        Self.dbManager.addItemMetadata(metadata)
        XCTAssertTrue(Self.dbManager.markItemAsExcludedFromSync(ocId: metadata.ocId))

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertNil(error)
        XCTAssertTrue(rootItem.children.contains { $0.identifier == remoteBundle.identifier })
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: metadata.ocId)?.deleted, true)
        XCTAssertFalse(Self.dbManager.isItemExcludedFromSync(ocId: metadata.ocId))

        let read = await Enumerator.readServerUrl(rootItem.remotePath, account: Self.account, remoteInterface: remoteInterface, dbManager: Self.dbManager, log: FileProviderLogMock())
        XCTAssertNil(read.error)
        XCTAssertTrue(try XCTUnwrap(read.metadatas).contains { $0.ocId == metadata.ocId })
        XCTAssertFalse(try XCTUnwrap(read.changes).createdAndUpdated.contains { $0.ocId == metadata.ocId })
        XCTAssertTrue(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: metadata.ocId)).deleted)
        XCTAssertTrue(rootItem.children.contains { $0.identifier == remoteBundle.identifier })
    }

    func testDeleteIsRefusedWhenTheExclusionLookupFails() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteFile = MockRemoteItem(
            identifier: "lookup-failure-id",
            name: "Keep.txt",
            remotePath: Self.account.davFilesUrl + "/Keep.txt",
            data: Data("keep".utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteFile.parent = rootItem
        rootItem.children = [remoteFile]
        let metadata = remoteFile.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(metadata)
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        // The deletion cannot tell whether it must stay local, so it must not reach the server.
        try Self.dbManager.breakTableForTesting(ExcludedFromSyncItemRecord.databaseTableName)
        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertEqual((error as? NSError)?.code, NSFileProviderError.cannotSynchronize.rawValue)
        XCTAssertTrue(rootItem.children.contains { $0.identifier == remoteFile.identifier }, "The server copy survives a refused deletion.")

        // The table is gone for good in this store; give the following tests a fresh one.
        Self.dbManager.recreateTablesForTesting()
    }

    func testDeletePreconditionFailureKeepsIncompleteChunkUpload() async throws {
        let remoteInterface = MockRemoteInterface(
            account: Self.account,
            rootItem: rootItem,
            rootTrashItem: rootTrashItem
        )
        let itemIdentifier = "file-with-failed-delete"
        let remoteItem = MockRemoteItem(
            identifier: itemIdentifier,
            name: "changed-since-read.txt",
            remotePath: Self.account.davFilesUrl + "/changed-since-read.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteItem.parent = rootItem
        rootItem.children = [remoteItem]
        remoteInterface.deleteError = NKError(statusCode: 412, fallbackDescription: "Precondition Failed")
        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let chunkUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: 8,
            modificationDate: Date(timeIntervalSince1970: 1_700_000_000)
        )
        let chunksDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("failed-delete-chunks-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: chunksDirectory, withIntermediateDirectories: true)
        try Data([1]).write(to: chunksDirectory.appendingPathComponent("2"))
        remoteInterface.chunkUploadDirectories[chunkUploadId] = chunksDirectory
        defer { try? FileManager.default.removeItem(at: chunksDirectory) }

        Self.dbManager.addRemoteFileChunks([
            RemoteFileChunk(
                fileName: "2",
                size: 3,
                remoteChunkStoreFolderName: chunkUploadId
            )
        ])

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await item.delete(dbManager: Self.dbManager)

        XCTAssertEqual((error as? CocoaError)?.code, .fileWriteUnknown)
        XCTAssertEqual(remoteInterface.lastDeleteRemotePath, remoteItem.remotePath)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: itemIdentifier)?.deleted, false)
        XCTAssertTrue(FileManager.default.fileExists(atPath: chunksDirectory.path))
        XCTAssertEqual(
            Self.dbManager.remoteFileChunks(uploadId: chunkUploadId)?.count,
            1
        )
    }

    func testDeleteFolderAndContents() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteFolder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteItem = MockRemoteItem(
            identifier: "file",
            name: "file",
            remotePath: Self.account.davFilesUrl + "/folder/file",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItem]
        remoteItem.parent = remoteFolder

        XCTAssertFalse(rootItem.children.isEmpty)
        XCTAssertFalse(remoteFolder.children.isEmpty)

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        let remoteItemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(remoteItemMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItem.identifier))

        let chunkUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: remoteItem.identifier,
            fileSize: 8,
            modificationDate: Date(timeIntervalSince1970: 1_700_000_000)
        )
        let chunksDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("deleted-descendant-chunks-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: chunksDirectory, withIntermediateDirectories: true)
        try Data([1]).write(to: chunksDirectory.appendingPathComponent("2"))
        remoteInterface.chunkUploadDirectories[chunkUploadId] = chunksDirectory
        defer { try? FileManager.default.removeItem(at: chunksDirectory) }

        Self.dbManager.addRemoteFileChunks([
            RemoteFileChunk(
                fileName: "2",
                size: 3,
                remoteChunkStoreFolderName: chunkUploadId
            )
        ])

        let folder = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await folder.delete(dbManager: Self.dbManager)
        XCTAssertNil(error)
        XCTAssertTrue(rootItem.children.isEmpty)

        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertFalse(FileManager.default.fileExists(atPath: chunksDirectory.path))
        XCTAssertEqual(
            Self.dbManager.remoteFileChunks(uploadId: chunkUploadId)?.count,
            0
        )
    }

    func testDeleteFolderPreservesPendingDescendantChunkUpload() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteFolder = MockRemoteItem(
            identifier: "folder-with-pending-upload",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemIdentifier = "pending-descendant"
        let remoteItem = MockRemoteItem(
            identifier: itemIdentifier,
            name: "pending.txt",
            remotePath: Self.account.davFilesUrl + "/folder/pending.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItem]
        remoteItem.parent = remoteFolder

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        var itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        itemMetadata.status = Status.uploading.rawValue
        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemMetadata)

        let chunkUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: remoteItem.identifier,
            fileSize: 8,
            modificationDate: Date(timeIntervalSince1970: 1_700_000_000)
        )
        setChunkUploadIdentifier(
            uploadIdentifier: chunkUploadId,
            itemIdentifier: remoteItem.identifier,
            dbManager: Self.dbManager,
            logger: FileProviderLogger(category: "ItemDeleteTests", log: FileProviderLogMock())
        )
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: itemIdentifier)?.status, Status.uploading.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: itemIdentifier)?.chunkUploadId, chunkUploadId)

        let chunksDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("preserved-descendant-chunks-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: chunksDirectory, withIntermediateDirectories: true)
        try Data([1]).write(to: chunksDirectory.appendingPathComponent("2"))
        remoteInterface.chunkUploadDirectories[chunkUploadId] = chunksDirectory
        defer { try? FileManager.default.removeItem(at: chunksDirectory) }

        Self.dbManager.addRemoteFileChunks([
            RemoteFileChunk(
                fileName: "2",
                size: 3,
                remoteChunkStoreFolderName: chunkUploadId
            )
        ])

        let folder = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await folder.delete(dbManager: Self.dbManager)

        XCTAssertNil(error)
        XCTAssertTrue(FileManager.default.fileExists(atPath: chunksDirectory.path))
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: itemIdentifier)?.status, Status.uploading.rawValue)
        XCTAssertEqual(
            Self.dbManager.itemMetadata(ocId: itemIdentifier)?.chunkUploadId,
            chunkUploadId
        )
        XCTAssertEqual(
            Self.dbManager.remoteFileChunks(uploadId: chunkUploadId)?.count,
            1
        )
    }

    func testDeleteWithTrashing() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let itemIdentifier = "file"
        let remoteItem = MockRemoteItem(
            identifier: itemIdentifier,
            name: "file",
            remotePath: Self.account.davFilesUrl + "/file",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteItem.parent = rootItem
        rootItem.children = [remoteItem]

        XCTAssertFalse(rootItem.children.isEmpty)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        XCTAssertEqual(itemMetadata.isTrashed, false)

        Self.dbManager.addItemMetadata(itemMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: itemIdentifier))

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await item.delete(trashing: true, dbManager: Self.dbManager)
        XCTAssertNil(error)
        XCTAssertTrue(rootItem.children.isEmpty)

        let postTrashingMetadata = Self.dbManager.itemMetadata(ocId: itemIdentifier)
        XCTAssertNotNil(postTrashingMetadata)
        XCTAssertEqual(postTrashingMetadata?.serverUrl, Self.account.trashUrl)
        XCTAssertEqual(
            try Self.dbManager.parentItemIdentifierFromMetadata(XCTUnwrap(postTrashingMetadata)), .trashContainer
        )
        XCTAssertEqual(postTrashingMetadata?.isTrashed, true)
        XCTAssertEqual(postTrashingMetadata?.trashbinFileName, "file") // Remember we need to sync
        XCTAssertEqual(postTrashingMetadata?.trashbinOriginalLocation, "file")
    }

    func testDeleteDoesNotPropagateIgnoredFile() async throws {
        let ignoredMatcher = IgnoredFilesMatcher(ignoreList: ["*.log", "/tmp/"], log: FileProviderLogMock())
        let remoteFile = MockRemoteItem(
            identifier: "ignored-file-id",
            name: "debug.log",
            remotePath: Self.account.davFilesUrl + "/debug.log",
            data: Data("ignored contents".utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteFile.parent = rootItem
        rootItem.children = [remoteFile]
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let metadata = remoteFile.toNKFile().toItemMetadata()
        Self.dbManager.addItemMetadata(metadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: metadata.ocId))
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let error = await item.delete(
            trashing: false,
            domain: nil,
            ignoredFiles: ignoredMatcher,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: metadata.ocId)?.deleted, true)

        let read = await Enumerator.readServerUrl(rootItem.remotePath, account: Self.account, remoteInterface: remoteInterface, dbManager: Self.dbManager, log: FileProviderLogMock())
        XCTAssertNil(read.error)
        XCTAssertTrue(try XCTUnwrap(read.metadatas).contains { $0.ocId == metadata.ocId })
        XCTAssertFalse(try XCTUnwrap(read.changes).createdAndUpdated.contains { $0.ocId == metadata.ocId })
        XCTAssertTrue(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: metadata.ocId)).deleted)
        XCTAssertTrue(rootItem.children.contains { $0.identifier == remoteFile.identifier })
    }

    func testDeleteLockFileUnlocksTargetFile() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let scenario = makeLockFileDeletionScenario(
            targetFileName: "MyDoc.odt",
            remoteInterface: remoteInterface
        )

        let error = await scenario.lockItem.delete(dbManager: Self.dbManager)

        XCTAssertEqual(
            Self.dbManager.itemMetadata(ocId: scenario.lockFileMetadata.ocId)?.deleted,
            true
        )
        XCTAssertNil(error)
        XCTAssertFalse(
            scenario.targetRemote.locked,
            "Expected the target file to be unlocked after lock file deletion"
        )
    }

    func testDeleteLockFileTreatsPreconditionFailureAsAlreadyUnlocked() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.lockUnlockError = NKError(
            errorCode: 412,
            errorDescription: "Precondition Failed"
        )
        let scenario = makeLockFileDeletionScenario(
            targetFileName: "AlreadyUnlocked.odt",
            remoteInterface: remoteInterface
        )

        let error = await scenario.lockItem.delete(dbManager: Self.dbManager)

        XCTAssertNil(error)
        XCTAssertEqual(
            Self.dbManager.itemMetadata(ocId: scenario.lockFileMetadata.ocId)?.deleted,
            true
        )
        let targetMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: scenario.targetMetadata.ocId)
        )
        XCTAssertFalse(targetMetadata.lock)
        XCTAssertNil(targetMetadata.lockOwner)
        XCTAssertNil(targetMetadata.lockOwnerDisplayName)
        XCTAssertNil(targetMetadata.lockOwnerEditor)
        XCTAssertNil(targetMetadata.lockOwnerType)
        XCTAssertNil(targetMetadata.lockTime)
        XCTAssertNil(targetMetadata.lockTimeOut)
        XCTAssertNil(targetMetadata.lockToken)
    }

    func testDeleteLockFileDoesNotIgnoreLockedResponse() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.lockUnlockError = NKError(errorCode: 423, errorDescription: "Locked")
        let scenario = makeLockFileDeletionScenario(
            targetFileName: "LockedBySomeoneElse.odt",
            remoteInterface: remoteInterface
        )

        let error = await scenario.lockItem.delete(dbManager: Self.dbManager)

        XCTAssertNotNil(error)
        XCTAssertEqual(
            Self.dbManager.itemMetadata(ocId: scenario.lockFileMetadata.ocId)?.deleted,
            true
        )
        let targetMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: scenario.targetMetadata.ocId)
        )
        XCTAssertTrue(targetMetadata.lock)
        XCTAssertEqual(targetMetadata.lockToken, "opaquelocktoken:test-token")
    }

    func testDeleteLockFileWithoutCapabilitiesRemovesLocalMetadataButKeepsServerLock() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        XCTAssert(remoteInterface.capabilities.contains(##""locking": "1.0","##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""locking": "1.0","##, with: "")

        // Setup remote folder and file
        let folderRemote = MockRemoteItem(
            identifier: "folder-id",
            versionIdentifier: "1",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        let targetFileName = "MyDoc.odt"
        let targetRemote = MockRemoteItem(
            identifier: "folder/\(targetFileName)",
            versionIdentifier: "1",
            name: targetFileName,
            remotePath: folderRemote.remotePath + "/" + targetFileName,
            data: Data("test data".utf8),
            locked: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        folderRemote.children = [targetRemote]
        folderRemote.parent = rootItem
        rootItem.children = [folderRemote]

        // Insert folder and target file into DB
        var folderMetadata = SendableItemMetadata(
            ocId: folderRemote.identifier, fileName: "folder", account: Self.account
        )
        folderMetadata.directory = true
        Self.dbManager.addItemMetadata(folderMetadata)

        var targetMetadata = SendableItemMetadata(
            ocId: targetRemote.identifier, fileName: targetFileName, account: Self.account
        )
        targetMetadata.serverUrl += "/folder"
        Self.dbManager.addItemMetadata(targetMetadata)

        // Construct the lock file metadata (used in deletion)
        let lockFileName = ".~lock.\(targetFileName)#"
        var lockFileMetadata = SendableItemMetadata(
            ocId: "lock-id", fileName: lockFileName, account: Self.account
        )
        lockFileMetadata.serverUrl += "/folder"
        Self.dbManager.addItemMetadata(lockFileMetadata)

        let lockItem = Item(
            metadata: lockFileMetadata,
            parentItemIdentifier: .init(folderMetadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        // Delete the lock file
        let error = await lockItem.delete(dbManager: Self.dbManager)
        // The local lock metadata is always removed, even without locking capability, so it is
        // never orphaned in the database.
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: lockFileMetadata.ocId)?.deleted, true)
        XCTAssertNil(error)
        // No unlock request can be made without the capability, so the server lock is untouched.
        XCTAssertTrue(
            targetRemote.locked, "Expected the target file to still be locked"
        )
    }

    /// An Adobe lock file resolves its guarded document by sibling lookup, then unlocks it on the
    /// server on deletion, mirroring the Office/LibreOffice behaviour.
    func testDeleteAdobeLockFileUnlocksDocument() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let folderRemote = MockRemoteItem(
            identifier: "folder-id",
            versionIdentifier: "1",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        let targetFileName = "MyDoc.indd"
        let targetRemote = MockRemoteItem(
            identifier: "folder/\(targetFileName)",
            versionIdentifier: "1",
            name: targetFileName,
            remotePath: folderRemote.remotePath + "/" + targetFileName,
            data: Data("test data".utf8),
            locked: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        folderRemote.children = [targetRemote]
        folderRemote.parent = rootItem
        rootItem.children = [folderRemote]

        var folderMetadata = SendableItemMetadata(
            ocId: folderRemote.identifier, fileName: "folder", account: Self.account
        )
        folderMetadata.directory = true
        Self.dbManager.addItemMetadata(folderMetadata)

        var targetMetadata = SendableItemMetadata(
            ocId: targetRemote.identifier, fileName: targetFileName, account: Self.account
        )
        targetMetadata.serverUrl += "/folder"
        Self.dbManager.addItemMetadata(targetMetadata)

        let lockFileName = "~MyDoc~0kjyv(.idlk"
        var lockFileMetadata = SendableItemMetadata(
            ocId: "lock-id", fileName: lockFileName, account: Self.account
        )
        lockFileMetadata.serverUrl += "/folder"
        lockFileMetadata.isLockFileOfLocalOrigin = true
        Self.dbManager.addItemMetadata(lockFileMetadata)

        let lockItem = Item(
            metadata: lockFileMetadata,
            parentItemIdentifier: .init(folderMetadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await lockItem.delete(dbManager: Self.dbManager)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: lockFileMetadata.ocId)?.deleted, true)
        XCTAssertNil(error)
        XCTAssertFalse(
            targetRemote.locked, "Expected the document to be unlocked after lock file deletion"
        )
    }

    func testDeleteAutoCADLockFileWithSiblingKeepsDocumentLocked() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let folderRemote = MockRemoteItem(
            identifier: "folder-id",
            versionIdentifier: "1",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        let targetFileName = "Drawing.dwg"
        let targetRemote = MockRemoteItem(
            identifier: "folder/\(targetFileName)",
            versionIdentifier: "1",
            name: targetFileName,
            remotePath: folderRemote.remotePath + "/" + targetFileName,
            data: Data("test data".utf8),
            locked: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        folderRemote.children = [targetRemote]
        folderRemote.parent = rootItem
        rootItem.children = [folderRemote]

        var folderMetadata = SendableItemMetadata(
            ocId: folderRemote.identifier, fileName: "folder", account: Self.account
        )
        folderMetadata.directory = true
        Self.dbManager.addItemMetadata(folderMetadata)

        var targetMetadata = SendableItemMetadata(
            ocId: targetRemote.identifier, fileName: targetFileName, account: Self.account
        )
        targetMetadata.serverUrl += "/folder"
        Self.dbManager.addItemMetadata(targetMetadata)

        // Insert both .dwl and .dwl2 lock file metadata into the DB.
        var dwlMetadata = SendableItemMetadata(
            ocId: "dwl-id", fileName: "Drawing.dwl", account: Self.account
        )
        dwlMetadata.serverUrl += "/folder"
        dwlMetadata.isLockFileOfLocalOrigin = true
        Self.dbManager.addItemMetadata(dwlMetadata)

        var dwl2Metadata = SendableItemMetadata(
            ocId: "dwl2-id", fileName: "Drawing.dwl2", account: Self.account
        )
        dwl2Metadata.serverUrl += "/folder"
        dwl2Metadata.isLockFileOfLocalOrigin = true
        Self.dbManager.addItemMetadata(dwl2Metadata)

        // Delete .dwl2 while .dwl still exists — document must stay locked.
        let dwl2Item = Item(
            metadata: dwl2Metadata,
            parentItemIdentifier: .init(folderMetadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await dwl2Item.delete(dbManager: Self.dbManager)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: dwl2Metadata.ocId)?.deleted, true)
        XCTAssertNil(error)
        XCTAssertTrue(
            targetRemote.locked, "Expected the document to stay locked while .dwl sibling exists"
        )

        // Now delete .dwl — with both gone, the document must unlock.
        let dwlItem = Item(
            metadata: dwlMetadata,
            parentItemIdentifier: .init(folderMetadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error2 = await dwlItem.delete(dbManager: Self.dbManager)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: dwlMetadata.ocId)?.deleted, true)
        XCTAssertNil(error2)
        XCTAssertFalse(
            targetRemote.locked, "Expected the document to be unlocked after both lock files are deleted"
        )
    }

    func testFailOnNonRecursiveNonEmptyDirDelete() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteFolder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteItem = MockRemoteItem(
            identifier: "file",
            name: "file",
            remotePath: Self.account.davFilesUrl + "/folder/file",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItem]
        remoteItem.parent = remoteFolder

        XCTAssertFalse(rootItem.children.isEmpty)
        XCTAssertFalse(remoteFolder.children.isEmpty)

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        let remoteItemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(remoteItemMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItem.identifier))

        let folder = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let error = await folder.delete(options: [], dbManager: Self.dbManager)
        XCTAssertNotNil(error)
        XCTAssertEqual(error as? NSFileProviderError?, NSFileProviderError(.directoryNotEmpty))
        XCTAssertFalse(rootItem.children.isEmpty)

        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItem.identifier))
    }
}

@Test(arguments: [
    ("/instance//nextcloud", "/instance//nextcloud", true),
    ("/instance/./nextcloud", "/instance/nextcloud", true),
    ("/instance/previous/../nextcloud", "/instance/nextcloud", true),
    ("/instance//nextcloud", "/instance//nextcloud", false)
])
func deletesItemReturnedAtNormalizedDAVURL(
    configuredWebroot: String,
    responseWebroot: String,
    metadataMatchesAccount: Bool
) async throws {
    let host = UUID().uuidString.lowercased() + ".example.invalid"
    let origin = "https://" + host
    let account = Account(
        user: "login",
        id: "user-id",
        serverUrl: origin + configuredWebroot,
        password: "test"
    )
    let filePath = Account.webDavFilesUrlSuffix + account.id + "/Docs/report.txt"
    let configuredURL = try #require(URL(string: account.serverUrl + filePath))
    let parsedURL = try #require(URL(string: origin + "/instance/nextcloud" + filePath))
    let itemID = "report-id"
    let body = Data("""
    <d:multistatus xmlns:d="DAV:" xmlns:oc="http://owncloud.org/ns">
      <d:response>
        <d:href>\(responseWebroot)\(filePath)</d:href>
        <d:propstat>
          <d:prop>
            <oc:id>\(itemID)</oc:id>
            <d:getetag>"file-v1"</d:getetag>
            <d:getcontenttype>text/plain</d:getcontenttype>
          </d:prop>
          <d:status>HTTP/1.1 200 OK</d:status>
        </d:propstat>
      </d:response>
    </d:multistatus>
    """.utf8)
    let requests = OSAllocatedUnfairLock(initialState: [URLRequest]())
    MockHTTPURLProtocol.setHandlers(
        forHost: host,
        start: { request in
            requests.withLock { $0.append(request.request) }
            let url = request.request.url
            switch request.request.httpMethod {
                case "PROPFIND" where url == configuredURL || url == parsedURL:
                    request.respond(status: 207, headers: ["Content-Type": "application/xml"], body: body)
                case "DELETE" where url == configuredURL || url == parsedURL:
                    request.respond(status: 204)
                default:
                    request.respond(status: 405)
            }
        },
        stop: { _ in }
    )
    defer { MockHTTPURLProtocol.removeHandlers(forHost: host) }

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
    let file = try #require(initialRead.files.first)
    try #require(file.serverUrl + "/" + file.fileName == parsedURL.absoluteString)
    var metadata = file.toItemMetadata()
    try #require(metadata.urlBase == account.serverUrl)
    let targetURL = try #require(URL(string: metadata.remotePath()))
    try #require(targetURL == configuredURL || targetURL == parsedURL)

    let manager = DatabaseTestSuites.makeManager(account: account)
    manager.addItemMetadata(metadata)
    if !metadataMatchesAccount {
        // This URL could also have come from a cache created before the account URL changed.
        metadata.urlBase = origin + "/instance/nextcloud"
    }
    let item = Item(
        metadata: metadata,
        parentItemIdentifier: .init("docs-id"),
        account: account,
        remoteInterface: remote,
        dbManager: manager
    )
    requests.withLock { $0.removeAll() }
    let error = await item.delete(dbManager: manager)
    let deleteRequests = requests.withLock { $0 }

    if metadataMatchesAccount {
        #expect(error == nil)
        #expect(deleteRequests.map(\.url) == [targetURL, targetURL])
        #expect(deleteRequests.map(\.httpMethod) == ["PROPFIND", "DELETE"])
        #expect(deleteRequests.first?.value(forHTTPHeaderField: "Depth") == "0")
        #expect(deleteRequests.last?.value(forHTTPHeaderField: "If-Match") == "\"file-v1\"")
    } else {
        #expect((error as? NSFileProviderError)?.code == .serverUnreachable)
        #expect(deleteRequests.isEmpty)
    }
    #expect(manager.itemMetadata(ocId: itemID)?.deleted == metadataMatchesAccount)
}

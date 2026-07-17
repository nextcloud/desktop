//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
import TestInterface
import XCTest

final class ItemDeleteTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )
    lazy var rootItem = MockRemoteItem.rootItem(account: Self.account)
    lazy var rootTrashItem = MockRemoteItem.rootTrashItem(account: Self.account)
    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    override func tearDown() {
        rootItem.children = []
        rootTrashItem.children = []
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

        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: itemIdentifier)?.deleted, true)
    }

    func testDeleteFolderAndContents() async {
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

        let error = await folder.delete(dbManager: Self.dbManager)
        XCTAssertNil(error)
        XCTAssertTrue(rootItem.children.isEmpty)

        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteItem.identifier))
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

    func testDeleteDoesNotPropagateIgnoredFile() async {
        let ignoredMatcher = IgnoredFilesMatcher(ignoreList: ["*.log", "/tmp/"], log: FileProviderLogMock())
        let metadata = SendableItemMetadata(
            ocId: "ignored-file-id",
            fileName: "debug.log",
            account: Self.account
        )
        Self.dbManager.addItemMetadata(metadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: metadata.ocId))
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account, rootItem: rootItem),
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
    }

    func testDeleteLockFileUnlocksTargetFile() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

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
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: lockFileMetadata.ocId)?.deleted, true)

        // Assert: no error returned
        XCTAssertNil(error)

        // Assert: remote file is now unlocked
        XCTAssertFalse(
            targetRemote.locked, "Expected the target file to be unlocked after lock file deletion"
        )
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

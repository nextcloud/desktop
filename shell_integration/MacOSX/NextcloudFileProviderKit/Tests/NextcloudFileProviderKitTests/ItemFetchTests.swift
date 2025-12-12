//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
import TestInterface
import XCTest

final class ItemFetchTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    lazy var rootItem = MockRemoteItem.rootItem(account: Self.account)
    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    override func tearDown() {
        rootItem.children = []
    }

    func testFetchFileContents() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.injectMock(Self.account)

        let remoteItem = MockRemoteItem(
            identifier: "item",
            versionIdentifier: "0",
            name: "item.txt",
            remotePath: Self.account.davFilesUrl + "/item.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteItem]
        remoteItem.parent = rootItem

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)
        XCTAssertNotNil(itemMetadata.ocId)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (localPathMaybe, fetchedItemMaybe, error) = await item.fetchContents(dbManager: Self.dbManager)
        XCTAssertNil(error)
        let localPath = try XCTUnwrap(localPathMaybe)
        let fetchedItem = try XCTUnwrap(fetchedItemMaybe)
        let contents = try Data(contentsOf: localPath)

        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId))

        XCTAssertEqual(contents, remoteItem.data)
        XCTAssertTrue(fetchedItem.isDownloaded)
        XCTAssertTrue(fetchedItem.isUploaded)
        XCTAssertEqual(fetchedItem.itemIdentifier, item.itemIdentifier)
        XCTAssertEqual(fetchedItem.filename, item.filename)
        XCTAssertEqual(fetchedItem.creationDate, item.creationDate)
    }

    func testFetchDirectoryContents() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.injectMock(Self.account)

        let remoteDirectory = MockRemoteItem(
            identifier: "directory",
            versionIdentifier: "0",
            name: "directory",
            remotePath: Self.account.davFilesUrl + "/directory",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteDirectoryChildFile = MockRemoteItem(
            identifier: "childFile",
            versionIdentifier: "0",
            name: "file.txt",
            remotePath: remoteDirectory.remotePath + "/file.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteDirectoryChildDirA = MockRemoteItem(
            identifier: "childDirectoryA",
            versionIdentifier: "0",
            name: "directoryA",
            remotePath: remoteDirectory.remotePath + "/directoryA",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteDirectoryChildDirB = MockRemoteItem(
            identifier: "childDirectoryB",
            versionIdentifier: "0",
            name: "directoryB",
            remotePath: remoteDirectory.remotePath + "/directoryB",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteDirectoryChildDirBChildFile = MockRemoteItem(
            identifier: "childDirectoryBChildFile",
            versionIdentifier: "0",
            name: "dirBfile.txt",
            remotePath: remoteDirectoryChildDirB.remotePath + "/dirBfile.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteDirectory]
        remoteDirectory.parent = rootItem
        remoteDirectory.children = [
            remoteDirectoryChildFile, remoteDirectoryChildDirA, remoteDirectoryChildDirB
        ]
        remoteDirectoryChildFile.parent = remoteDirectory
        remoteDirectoryChildDirA.parent = remoteDirectory
        remoteDirectoryChildDirB.parent = remoteDirectory
        remoteDirectoryChildDirB.children = [remoteDirectoryChildDirBChildFile]
        remoteDirectoryChildDirBChildFile.parent = remoteDirectoryChildDirB

        let directoryMetadata = remoteDirectory.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryMetadata)

        let directoryChildFileMetadata =
            remoteDirectoryChildFile.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryChildFileMetadata)

        let directoryChildDirAMetadata =
            remoteDirectoryChildDirA.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryChildDirAMetadata)

        let directoryChildDirBMetadata =
            remoteDirectoryChildDirB.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryChildDirBMetadata)

        let directoryChildDirBChildFileMetadata =
            remoteDirectoryChildDirBChildFile.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryChildDirBChildFileMetadata)

        let item = Item(
            metadata: directoryMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (localPathMaybe, fetchedItemMaybe, error) =
            await item.fetchContents(dbManager: Self.dbManager)
        XCTAssertNil(error)
        let localPath = try XCTUnwrap(localPathMaybe)
        let fetchedItem = try XCTUnwrap(fetchedItemMaybe)

        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: directoryMetadata.ocId))

        XCTAssertEqual(fetchedItem.itemIdentifier, item.itemIdentifier)
        XCTAssertEqual(fetchedItem.filename, item.filename)
        XCTAssertEqual(fetchedItem.creationDate, item.creationDate)
        XCTAssertTrue(fetchedItem.isUploaded)
        XCTAssertTrue(fetchedItem.isDownloaded)

        let fm = FileManager.default
        var itemIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(atPath: localPath.path, isDirectory: &itemIsDir))
        XCTAssertTrue(itemIsDir.boolValue)

        let itemChildFileUrl = localPath.appendingPathComponent("file.txt")
        let itemChildFilePath = itemChildFileUrl.path
        var itemChildFileIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(atPath: itemChildFilePath, isDirectory: &itemChildFileIsDir))
        XCTAssertFalse(itemChildFileIsDir.boolValue)
        XCTAssertEqual(try Data(contentsOf: itemChildFileUrl), remoteDirectoryChildFile.data)

        let itemChildDirAPath = localPath.appendingPathComponent("directoryA").path
        var itemChildDirAIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(atPath: itemChildDirAPath, isDirectory: &itemChildDirAIsDir))
        XCTAssertTrue(itemChildDirAIsDir.boolValue)

        let itemChildDirBUrl = localPath.appendingPathComponent("directoryB")
        let itemChildDirBPath = itemChildDirBUrl.path
        var itemChildDirBIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(atPath: itemChildDirBPath, isDirectory: &itemChildDirBIsDir))
        XCTAssertTrue(itemChildDirBIsDir.boolValue)

        let itemChildDirBChildFileUrl = itemChildDirBUrl.appendingPathComponent("dirBfile.txt")
        let itemChildDirBChildFilePath = itemChildDirBChildFileUrl.path
        var itemChildDirBChildFileIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(
            atPath: itemChildDirBChildFilePath, isDirectory: &itemChildDirBChildFileIsDir
        ))
        XCTAssertFalse(itemChildDirBChildFileIsDir.boolValue)
        XCTAssertEqual(
            try Data(contentsOf: itemChildDirBChildFileUrl), remoteDirectoryChildDirBChildFile.data
        )
    }
}

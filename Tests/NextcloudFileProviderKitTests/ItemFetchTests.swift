//
//  ItemFetchTests.swift
//
//
//  Created by Claudio Cambra on 14/5/24.
//

import FileProvider
import NextcloudKit
import RealmSwift
import TestInterface
import XCTest
@testable import NextcloudFileProviderKit

final class ItemFetchTests: XCTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    lazy var rootItem = MockRemoteItem.rootItem(account: Self.account)
    static let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    override func tearDown() {
        rootItem.children = []
    }

    func testFetchFileContents() async throws {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)
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

        var itemMetadata = SendableItemMetadata()
        itemMetadata.ocId = remoteItem.identifier
        itemMetadata.etag = remoteItem.versionIdentifier
        itemMetadata.name = remoteItem.name
        itemMetadata.fileName = remoteItem.name
        itemMetadata.fileNameView = remoteItem.name
        itemMetadata.serverUrl = Self.account.davFilesUrl
        itemMetadata.urlBase = Self.account.serverUrl
        itemMetadata.account = Self.account.ncKitAccount
        itemMetadata.userId = Self.account.id
        itemMetadata.user = Self.account.username

        Self.dbManager.addItemMetadata(itemMetadata)
        XCTAssertNotNil(itemMetadata.ocId)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )

        let (localPathMaybe, fetchedItemMaybe, error) = await item.fetchContents(
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        let localPath = try XCTUnwrap(localPathMaybe)
        let fetchedItem = try XCTUnwrap(fetchedItemMaybe)
        let contents = try Data(contentsOf: localPath)

        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId))

        fetchedItem.dbManager = Self.dbManager

        XCTAssertEqual(contents, remoteItem.data)
        XCTAssertTrue(fetchedItem.isDownloaded)
        XCTAssertEqual(fetchedItem.itemIdentifier, item.itemIdentifier)
        XCTAssertEqual(fetchedItem.filename, item.filename)
        XCTAssertEqual(fetchedItem.creationDate, item.creationDate)
    }

    func testFetchDirectoryContents() async throws {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)
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

        var directoryMetadata = SendableItemMetadata()
        directoryMetadata.ocId = remoteDirectory.identifier
        directoryMetadata.etag = remoteDirectory.versionIdentifier
        directoryMetadata.name = remoteDirectory.name
        directoryMetadata.fileName = remoteDirectory.name
        directoryMetadata.fileNameView = remoteDirectory.name
        directoryMetadata.serverUrl = Self.account.davFilesUrl
        directoryMetadata.urlBase = Self.account.serverUrl
        directoryMetadata.account = Self.account.ncKitAccount
        directoryMetadata.userId = Self.account.id
        directoryMetadata.user = Self.account.username
        directoryMetadata.directory = true

        Self.dbManager.addItemMetadata(directoryMetadata)

        var directoryChildFileMetadata = SendableItemMetadata()
        directoryChildFileMetadata.ocId = remoteDirectoryChildFile.identifier
        directoryChildFileMetadata.etag = remoteDirectoryChildFile.versionIdentifier
        directoryChildFileMetadata.name = remoteDirectoryChildFile.name
        directoryChildFileMetadata.fileName = remoteDirectoryChildFile.name
        directoryChildFileMetadata.fileNameView = remoteDirectoryChildFile.name
        directoryChildFileMetadata.serverUrl =
            directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        directoryChildFileMetadata.urlBase = Self.account.serverUrl
        directoryChildFileMetadata.account = Self.account.ncKitAccount
        directoryChildFileMetadata.userId = Self.account.username
        directoryChildFileMetadata.user = Self.account.username
        directoryChildFileMetadata.contentType = "text/plain"
        directoryChildFileMetadata.size = Int64(remoteDirectoryChildFile.data?.count ?? 0)
        directoryChildFileMetadata.classFile = NKCommon.TypeClassFile.document.rawValue

        Self.dbManager.addItemMetadata(directoryChildFileMetadata)

        var directoryChildDirAMetadata = SendableItemMetadata()
        directoryChildDirAMetadata.ocId = remoteDirectoryChildDirA.identifier
        directoryChildDirAMetadata.etag = remoteDirectoryChildDirA.versionIdentifier
        directoryChildDirAMetadata.name = remoteDirectoryChildDirA.name
        directoryChildDirAMetadata.fileName = remoteDirectoryChildDirA.name
        directoryChildDirAMetadata.fileNameView = remoteDirectoryChildDirA.name
        directoryChildDirAMetadata.serverUrl =
            directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        directoryChildDirAMetadata.urlBase = Self.account.serverUrl
        directoryChildDirAMetadata.account = Self.account.ncKitAccount
        directoryChildDirAMetadata.userId = Self.account.id
        directoryChildDirAMetadata.user = Self.account.username
        directoryChildDirAMetadata.directory = true

        Self.dbManager.addItemMetadata(directoryChildDirAMetadata)

        var directoryChildDirBMetadata = SendableItemMetadata()
        directoryChildDirBMetadata.ocId = remoteDirectoryChildDirB.identifier
        directoryChildDirBMetadata.etag = remoteDirectoryChildDirB.versionIdentifier
        directoryChildDirBMetadata.name = remoteDirectoryChildDirB.name
        directoryChildDirBMetadata.fileName = remoteDirectoryChildDirB.name
        directoryChildDirBMetadata.fileNameView = remoteDirectoryChildDirB.name
        directoryChildDirBMetadata.serverUrl =
            directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        directoryChildDirBMetadata.urlBase = Self.account.serverUrl
        directoryChildDirBMetadata.account = Self.account.ncKitAccount
        directoryChildDirBMetadata.userId = Self.account.username
        directoryChildDirBMetadata.user = Self.account.username
        directoryChildDirBMetadata.directory = true

        Self.dbManager.addItemMetadata(directoryChildDirBMetadata)

        var directoryChildDirBChildFileMetadata = SendableItemMetadata()
        directoryChildDirBChildFileMetadata.ocId = remoteDirectoryChildDirBChildFile.identifier
        directoryChildDirBChildFileMetadata.etag =
            remoteDirectoryChildDirBChildFile.versionIdentifier
        directoryChildDirBChildFileMetadata.name = remoteDirectoryChildDirBChildFile.name
        directoryChildDirBChildFileMetadata.fileName = remoteDirectoryChildDirBChildFile.name
        directoryChildDirBChildFileMetadata.fileNameView = remoteDirectoryChildDirBChildFile.name
        directoryChildDirBChildFileMetadata.serverUrl =
            directoryChildDirBMetadata.serverUrl + "/" + directoryChildDirBMetadata.fileName
        directoryChildDirBChildFileMetadata.urlBase = Self.account.serverUrl
        directoryChildDirBChildFileMetadata.account = Self.account.ncKitAccount
        directoryChildDirBChildFileMetadata.userId = Self.account.id
        directoryChildDirBChildFileMetadata.user = Self.account.username
        directoryChildDirBChildFileMetadata.contentType = "text/plain"
        directoryChildDirBChildFileMetadata.size =
            Int64(remoteDirectoryChildDirBChildFile.data?.count ?? 0)
        directoryChildDirBChildFileMetadata.classFile = NKCommon.TypeClassFile.document.rawValue

        Self.dbManager.addItemMetadata(directoryChildDirBChildFileMetadata)

        let item = Item(
            metadata: directoryMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )
        item.dbManager = Self.dbManager

        let (localPathMaybe, fetchedItemMaybe, error) =
            await item.fetchContents(dbManager: Self.dbManager)
        XCTAssertNil(error)
        let localPath = try XCTUnwrap(localPathMaybe)
        let fetchedItem = try XCTUnwrap(fetchedItemMaybe)

        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: directoryMetadata.ocId))

        fetchedItem.dbManager = Self.dbManager

        XCTAssertEqual(fetchedItem.itemIdentifier, item.itemIdentifier)
        XCTAssertEqual(fetchedItem.filename, item.filename)
        XCTAssertEqual(fetchedItem.creationDate, item.creationDate)

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

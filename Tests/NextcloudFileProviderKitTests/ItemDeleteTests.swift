//
//  ItemDeleteTests.swift
//
//
//  Created by Claudio Cambra on 13/5/24.
//

import FileProvider
import NextcloudKit
import RealmSwift
import TestInterface
import XCTest
@testable import NextcloudFileProviderKit

final class ItemDeleteTests: XCTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )
    lazy var rootItem = MockRemoteItem(
        identifier: NSFileProviderItemIdentifier.rootContainer.rawValue,
        name: "root",
        remotePath: Self.account.davFilesUrl,
        directory: true,
        account: Self.account.ncKitAccount,
        username: Self.account.username,
        userId: Self.account.id,
        serverUrl: Self.account.serverUrl
    )
    static let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    override func tearDown() {
        rootItem.children = []
    }

    func testDeleteFile() async {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)
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

        let itemMetadata = ItemMetadata()
        itemMetadata.ocId = itemIdentifier

        Self.dbManager.addItemMetadata(itemMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(itemIdentifier))

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )

        let (error) = await item.delete(dbManager: Self.dbManager)
        XCTAssertNil(error)
        XCTAssertTrue(rootItem.children.isEmpty)

        XCTAssertNil(Self.dbManager.itemMetadataFromOcId(itemIdentifier))
    }

    func testDeleteFolderAndContents() async {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)
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

        let folderMetadata = ItemMetadata()
        folderMetadata.ocId = remoteFolder.identifier
        folderMetadata.fileName = remoteFolder.name
        folderMetadata.directory = true
        folderMetadata.serverUrl = Self.account.davFilesUrl

        let remoteItemMetadata = ItemMetadata()
        remoteItemMetadata.ocId = remoteItem.identifier
        remoteItemMetadata.fileName = remoteItem.name
        remoteItemMetadata.serverUrl = remoteFolder.remotePath

        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(remoteItemMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(remoteItem.identifier))

        let folder = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )

        let (error) = await folder.delete(dbManager: Self.dbManager)
        XCTAssertNil(error)
        XCTAssertTrue(rootItem.children.isEmpty)

        XCTAssertNil(Self.dbManager.itemMetadataFromOcId(remoteFolder.identifier))
        XCTAssertNil(Self.dbManager.itemMetadataFromOcId(remoteItem.identifier))
    }
}

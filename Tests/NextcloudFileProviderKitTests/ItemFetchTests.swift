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
        user: "testUser", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    lazy var rootItem = MockRemoteItem(
        identifier: NSFileProviderItemIdentifier.rootContainer.rawValue,
        name: "root",
        remotePath: Self.account.davFilesUrl,
        directory: true,
        account: Self.account.ncKitAccount,
        username: Self.account.username,
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

    func testFetchFileContents() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let remoteItem = MockRemoteItem(
            identifier: "item",
            versionIdentifier: "0",
            name: "item.txt",
            remotePath: Self.account.davFilesUrl + "/item.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteItem]
        remoteItem.parent = rootItem

        let itemMetadata = ItemMetadata()
        itemMetadata.ocId = remoteItem.identifier
        itemMetadata.etag = remoteItem.versionIdentifier
        itemMetadata.name = remoteItem.name
        itemMetadata.fileName = remoteItem.name
        itemMetadata.fileNameView = remoteItem.name
        itemMetadata.serverUrl = Self.account.davFilesUrl
        itemMetadata.urlBase = Self.account.serverUrl
        itemMetadata.account = Self.account.ncKitAccount
        itemMetadata.userId = Self.account.username
        itemMetadata.user = Self.account.username

        Self.dbManager.addItemMetadata(itemMetadata)
        XCTAssertNotNil(itemMetadata.ocId)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: remoteInterface
        )

        let (localPathMaybe, fetchedItemMaybe, error) = await item.fetchContents(
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        let localPath = try XCTUnwrap(localPathMaybe)
        let fetchedItem = try XCTUnwrap(fetchedItemMaybe)
        let contents = try Data(contentsOf: localPath)

        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(itemMetadata.ocId))

        fetchedItem.dbManager = Self.dbManager

        XCTAssertEqual(contents, remoteItem.data)
        XCTAssertTrue(fetchedItem.isDownloaded)
        XCTAssertEqual(fetchedItem.itemIdentifier, item.itemIdentifier)
        XCTAssertEqual(fetchedItem.filename, item.filename)
        XCTAssertEqual(fetchedItem.creationDate, item.creationDate)
    }
}

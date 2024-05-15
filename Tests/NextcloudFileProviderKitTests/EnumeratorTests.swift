//
//  EnumeratorTests.swift
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

final class EnumeratorTests: XCTestCase {
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
    lazy var remoteFolder = MockRemoteItem(
        identifier: "folder",
        versionIdentifier: "NEW",
        name: "folder",
        remotePath: Self.account.davFilesUrl + "/folder",
        directory: true,
        account: Self.account.ncKitAccount,
        username: Self.account.username,
        serverUrl: Self.account.serverUrl
    )
    lazy var remoteItemA = MockRemoteItem(
        identifier: "itemA",
        name: "itemA",
        remotePath: Self.account.davFilesUrl + "/folder/itemA",
        account: Self.account.ncKitAccount,
        username: Self.account.username,
        serverUrl: Self.account.serverUrl
    )
    lazy var remoteItemB = MockRemoteItem(
        identifier: "itemB",
        name: "itemB",
        remotePath: Self.account.davFilesUrl + "/folder/itemB",
        account: Self.account.ncKitAccount,
        username: Self.account.username,
        serverUrl: Self.account.serverUrl
    )

    static let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name

        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItemA, remoteItemB]
        remoteItemA.parent = remoteFolder
        remoteItemB.parent = remoteFolder
    }

    func testRootEnumeration() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            ncAccount: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 1)

        let retrievedFolderItem = try XCTUnwrap(observer.items.first)
        XCTAssertEqual(retrievedFolderItem.itemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedFolderItem.filename, remoteFolder.name)
        XCTAssertEqual(retrievedFolderItem.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(retrievedFolderItem.creationDate, remoteFolder.creationDate)
        XCTAssertEqual(retrievedFolderItem.contentModificationDate, remoteFolder.modificationDate)
    }

    func testFolderEnumeration() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let oldEtag = "OLD"
        let folderMetadata = ItemMetadata()
        folderMetadata.ocId = remoteFolder.identifier
        folderMetadata.etag = oldEtag
        folderMetadata.name = remoteFolder.name
        folderMetadata.fileName = remoteFolder.name
        folderMetadata.fileNameView = remoteFolder.name
        folderMetadata.serverUrl = Self.account.davFilesUrl
        folderMetadata.account = Self.account.ncKitAccount
        folderMetadata.user = Self.account.username
        folderMetadata.userId = Self.account.username
        folderMetadata.urlBase = Self.account.serverUrl

        Self.dbManager.addItemMetadata(folderMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(remoteFolder.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            ncAccount: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 2)

        // A pass of enumerating a target should update the target too. Let's check.
        let dbFolderMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadataFromOcId(remoteFolder.identifier)
        )
        let storedFolderItem = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteFolder.identifier),
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        storedFolderItem.dbManager = Self.dbManager
        XCTAssertEqual(dbFolderMetadata.etag, remoteFolder.versionIdentifier)
        XCTAssertNotEqual(dbFolderMetadata.etag, oldEtag)
        XCTAssertEqual(storedFolderItem.childItemCount?.intValue, remoteFolder.children.count)

        let retrievedItemA = try XCTUnwrap(
            observer.items.first(where: { $0.itemIdentifier.rawValue == remoteItemA.identifier })
        )
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(retrievedItemA.contentModificationDate, remoteItemA.modificationDate)
    }

    func testEnumerateFile() async throws {
        let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let folderMetadata = ItemMetadata()
        folderMetadata.ocId = remoteFolder.identifier
        folderMetadata.etag = remoteFolder.versionIdentifier
        folderMetadata.directory = true
        folderMetadata.name = remoteFolder.name
        folderMetadata.fileName = remoteFolder.name
        folderMetadata.fileNameView = remoteFolder.name
        folderMetadata.serverUrl = Self.account.davFilesUrl
        folderMetadata.account = Self.account.ncKitAccount
        folderMetadata.user = Self.account.username
        folderMetadata.userId = Self.account.username
        folderMetadata.urlBase = Self.account.serverUrl

        let itemAMetadata = ItemMetadata()
        itemAMetadata.ocId = remoteItemA.identifier
        itemAMetadata.etag = remoteItemA.versionIdentifier
        itemAMetadata.name = remoteItemA.name
        itemAMetadata.fileName = remoteItemA.name
        itemAMetadata.fileNameView = remoteItemA.name
        itemAMetadata.serverUrl = remoteFolder.remotePath
        itemAMetadata.account = Self.account.ncKitAccount
        itemAMetadata.user = Self.account.username
        itemAMetadata.userId = Self.account.username
        itemAMetadata.urlBase = Self.account.serverUrl

        dbManager.addItemMetadata(folderMetadata)
        dbManager.addItemMetadata(itemAMetadata)
        XCTAssertNotNil(dbManager.itemMetadataFromOcId(remoteFolder.identifier))
        XCTAssertNotNil(dbManager.itemMetadataFromOcId(remoteItemA.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteItemA.identifier),
            ncAccount: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 1)

        let retrievedItemAItem = try XCTUnwrap(observer.items.first)
        XCTAssertEqual(retrievedItemAItem.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemAItem.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemAItem.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemAItem.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(retrievedItemAItem.contentModificationDate, remoteItemA.modificationDate)
    }
}

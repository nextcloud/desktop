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
        versionIdentifier: "NEW",
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
    lazy var remoteItemC = MockRemoteItem(
        identifier: "itemC",
        name: "itemC",
        remotePath: Self.account.davFilesUrl + "/folder/itemC",
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
        remoteItemC.parent = nil
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

        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemAMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(remoteItemA.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteItemA.identifier),
            ncAccount: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
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

    func testFolderAndContentsChangeEnumeration() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        remoteFolder.children.removeAll(where: { $0.identifier == remoteItemB.identifier })
        remoteFolder.children.append(remoteItemC)
        remoteItemC.parent = remoteFolder

        let oldFolderEtag = "OLD"
        let folderMetadata = ItemMetadata()
        folderMetadata.ocId = remoteFolder.identifier
        folderMetadata.etag = oldFolderEtag
        folderMetadata.name = remoteFolder.name
        folderMetadata.fileName = remoteFolder.name
        folderMetadata.fileNameView = remoteFolder.name
        folderMetadata.serverUrl = Self.account.davFilesUrl
        folderMetadata.account = Self.account.ncKitAccount
        folderMetadata.user = Self.account.username
        folderMetadata.userId = Self.account.username
        folderMetadata.urlBase = Self.account.serverUrl

        let oldItemAEtag = "OLD"
        let itemAMetadata = ItemMetadata()
        itemAMetadata.ocId = remoteItemA.identifier
        itemAMetadata.etag = oldItemAEtag
        itemAMetadata.name = remoteItemA.name
        itemAMetadata.fileName = remoteItemA.name
        itemAMetadata.fileNameView = remoteItemA.name
        itemAMetadata.serverUrl = remoteFolder.remotePath
        itemAMetadata.account = Self.account.ncKitAccount
        itemAMetadata.user = Self.account.username
        itemAMetadata.userId = Self.account.username
        itemAMetadata.urlBase = Self.account.serverUrl

        let itemBMetadata = ItemMetadata()
        itemBMetadata.ocId = remoteItemB.identifier
        itemBMetadata.etag = remoteItemB.versionIdentifier
        itemBMetadata.name = remoteItemB.name
        itemBMetadata.fileName = remoteItemB.name
        itemBMetadata.fileNameView = remoteItemB.name
        itemBMetadata.serverUrl = remoteFolder.remotePath
        itemBMetadata.account = Self.account.ncKitAccount
        itemBMetadata.user = Self.account.username
        itemBMetadata.userId = Self.account.username
        itemBMetadata.urlBase = Self.account.serverUrl

        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemAMetadata)
        Self.dbManager.addItemMetadata(itemBMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(remoteItemA.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadataFromOcId(remoteItemB.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            ncAccount: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        // There are three changes: changed Item A, removed Item B, added Item C
        XCTAssertEqual(observer.changedItems.count, 2)
        XCTAssertTrue(observer.changedItems.contains(
            where: { $0.itemIdentifier.rawValue == remoteItemA.identifier }
        ))
        XCTAssertTrue(observer.changedItems.contains(
            where: { $0.itemIdentifier.rawValue == remoteItemC.identifier }
        ))
        XCTAssertEqual(observer.deletedItemIdentifiers.count, 1)
        XCTAssertTrue(observer.deletedItemIdentifiers.contains(
            where: { $0.rawValue == remoteItemB.identifier }
        ))

        // A pass of enumerating a target should update the target too. Let's check.
        let dbFolderMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadataFromOcId(remoteFolder.identifier)
        )
        let dbItemAMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadataFromOcId(remoteItemA.identifier)
        )
        let dbItemCMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadataFromOcId(remoteItemC.identifier)
        )
        XCTAssertNil(Self.dbManager.itemMetadataFromOcId(remoteItemB.identifier))
        XCTAssertEqual(dbFolderMetadata.etag, remoteFolder.versionIdentifier)
        XCTAssertNotEqual(dbFolderMetadata.etag, oldFolderEtag)
        XCTAssertEqual(dbItemAMetadata.etag, remoteItemA.versionIdentifier)
        XCTAssertNotEqual(dbItemAMetadata.etag, oldItemAEtag)

        let storedFolderItem = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteFolder.identifier),
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        storedFolderItem.dbManager = Self.dbManager

        let retrievedItemA = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemA.identifier }
        ))
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(retrievedItemA.contentModificationDate, remoteItemA.modificationDate)

        let retrievedItemC = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemC.identifier }
        ))
        XCTAssertEqual(retrievedItemC.itemIdentifier.rawValue, remoteItemC.identifier)
        XCTAssertEqual(retrievedItemC.filename, remoteItemC.name)
        XCTAssertEqual(retrievedItemC.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemC.creationDate, remoteItemC.creationDate)
        XCTAssertEqual(retrievedItemC.contentModificationDate, remoteItemC.modificationDate)
    }
}

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
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    var rootItem: MockRemoteItem!
    var remoteFolder: MockRemoteItem!
    var remoteItemA: MockRemoteItem!
    var remoteItemB: MockRemoteItem!
    var remoteItemC: MockRemoteItem!

    var rootTrashItem: MockRemoteItem!
    var remoteTrashItemA: MockRemoteItem!
    var remoteTrashItemB: MockRemoteItem!
    var remoteTrashItemC: MockRemoteItem!

    static let dbManager = FilesDatabaseManager(
        realmConfig: .defaultConfiguration, account: account
    )

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name

        rootItem = MockRemoteItem.rootItem(account: Self.account)

        remoteFolder = MockRemoteItem(
            identifier: "folder",
            versionIdentifier: "NEW",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteItemA = MockRemoteItem(
            identifier: "itemA",
            versionIdentifier: "NEW",
            name: "itemA",
            remotePath: Self.account.davFilesUrl + "/folder/itemA",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteItemB = MockRemoteItem(
            identifier: "itemB",
            name: "itemB",
            remotePath: Self.account.davFilesUrl + "/folder/itemB",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteItemC = MockRemoteItem(
            identifier: "itemC",
            name: "itemC",
            remotePath: Self.account.davFilesUrl + "/folder/itemC",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItemA, remoteItemB]
        remoteItemA.parent = remoteFolder
        remoteItemB.parent = remoteFolder
        remoteItemC.parent = nil

        rootTrashItem = MockRemoteItem.rootTrashItem(account: Self.account)

        remoteTrashItemA = MockRemoteItem(
            identifier: "trashItemA",
            name: "a.txt",
            remotePath: Self.account.trashUrl + "/a.txt",
            data: Data(repeating: 1, count: 32),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteTrashItemB = MockRemoteItem(
            identifier: "trashItemB",
            name: "b.txt",
            remotePath: Self.account.trashUrl + "/b.txt",
            data: Data(repeating: 1, count: 69),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteTrashItemC = MockRemoteItem(
            identifier: "trashItemC",
            name: "c.txt",
            remotePath: Self.account.trashUrl + "/c.txt",
            data: Data(repeating: 1, count: 100),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        rootTrashItem.children = [remoteTrashItemA, remoteTrashItemB, remoteTrashItemC]
        remoteTrashItemA.parent = rootTrashItem
        remoteTrashItemB.parent = rootTrashItem
        remoteTrashItemC.parent = rootTrashItem
    }

    func testRootEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
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
        XCTAssertEqual(
            Int(retrievedFolderItem.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteFolder.modificationDate.timeIntervalSince1970)
        )

        // Important to keep in mind. Default behaviour is fast enumeration, not deep enumeration
        let dbFolder = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertEqual(dbFolder.etag, "") // Folder is not visited yet, should not have etag
        XCTAssertEqual(dbFolder.fileName, remoteFolder.name)
        XCTAssertEqual(dbFolder.fileNameView, remoteFolder.name)
        XCTAssertEqual(dbFolder.serverUrl + "/" + dbFolder.fileName, remoteFolder.remotePath)
        XCTAssertEqual(dbFolder.account, Self.account.ncKitAccount)
        XCTAssertEqual(dbFolder.user, Self.account.username)
        XCTAssertEqual(dbFolder.userId, Self.account.id)
        XCTAssertEqual(dbFolder.urlBase, Self.account.serverUrl)

        let storedFolderItem = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteFolder.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        XCTAssertEqual(storedFolderItem.itemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(storedFolderItem.filename, remoteFolder.name)
        XCTAssertEqual(storedFolderItem.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(storedFolderItem.creationDate, remoteFolder.creationDate)
        XCTAssertEqual(
            Int(storedFolderItem.contentModificationDate?.timeIntervalSince1970 ?? 0),
            Int(remoteFolder.modificationDate.timeIntervalSince1970)
        )
        XCTAssertEqual(storedFolderItem.childItemCount?.intValue, 0) // Not visited yet, so no kids
    }

    func testWorkingSetEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 1) // Should only get the folder in root

        let retrievedFolderItem = try XCTUnwrap(observer.items.first)
        XCTAssertEqual(retrievedFolderItem.itemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedFolderItem.filename, remoteFolder.name)
        XCTAssertEqual(retrievedFolderItem.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(retrievedFolderItem.creationDate, remoteFolder.creationDate)
        XCTAssertEqual(
            Int(retrievedFolderItem.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteFolder.modificationDate.timeIntervalSince1970)
        )
        XCTAssertEqual(retrievedFolderItem.isUploaded, true)

        // Ensure the newly discovered folder has no etag
        let dbFolder = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertTrue(dbFolder.etag.isEmpty)
    }

    func testWorkingSetFastChangeEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 1) // Should only get the folder in root

        let retrievedFolderItem = try XCTUnwrap(observer.changedItems.first)
        XCTAssertEqual(retrievedFolderItem.itemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedFolderItem.filename, remoteFolder.name)
        XCTAssertEqual(retrievedFolderItem.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(retrievedFolderItem.creationDate, remoteFolder.creationDate)
        XCTAssertEqual(
            Int(retrievedFolderItem.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteFolder.modificationDate.timeIntervalSince1970)
        )
        XCTAssertEqual(retrievedFolderItem.isUploaded, true)

        // Ensure the newly discovered folder has no etag
        var dbFolder = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertTrue(dbFolder.etag.isEmpty)

        // Having an etag marks a folder as visited. 
        // We should get the two remaining files now, as the etag does not match the server but is
        // present, marking the folder as explored
        dbFolder.etag = "Not server etag"
        Self.dbManager.addItemMetadata(dbFolder)

        let newObserver = MockChangeObserver(enumerator: enumerator)
        try await newObserver.enumerateChanges()
        XCTAssertEqual(newObserver.changedItems.count, 3)

        let newNewObsever = MockChangeObserver(enumerator: enumerator)
        try await newNewObsever.enumerateChanges()
        XCTAssertEqual(newNewObsever.changedItems.count, 0)
    }

    func testWorkingSetSlowChangeEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            fastEnumeration: false
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 3) // Should get all items

        let retrievedFolderItem = try XCTUnwrap(observer.changedItems.first)
        XCTAssertEqual(retrievedFolderItem.itemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedFolderItem.filename, remoteFolder.name)
        XCTAssertEqual(retrievedFolderItem.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(retrievedFolderItem.creationDate, remoteFolder.creationDate)
        XCTAssertEqual(
            Int(retrievedFolderItem.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteFolder.modificationDate.timeIntervalSince1970)
        )
        XCTAssertEqual(retrievedFolderItem.isUploaded, true)

        // Ensure the newly discovered folder has an etag
        let dbFolder = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertEqual(dbFolder.etag, remoteFolder.versionIdentifier)
    }

    func testFolderEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        let oldEtag = "OLD"
        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = oldEtag

        Self.dbManager.addItemMetadata(folderMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 2)

        // A pass of enumerating a target should update the target too. Let's check.
        let dbFolderMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteFolder.identifier)
        )
        let storedFolderItem = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteFolder.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        XCTAssertEqual(dbFolderMetadata.etag, remoteFolder.versionIdentifier)
        XCTAssertNotEqual(dbFolderMetadata.etag, oldEtag)
        XCTAssertEqual(storedFolderItem.childItemCount?.intValue, remoteFolder.children.count)
        XCTAssertEqual(storedFolderItem.isUploaded, true)

        let retrievedItemA = try XCTUnwrap(
            observer.items.first(where: { $0.itemIdentifier.rawValue == remoteItemA.identifier })
        )
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(retrievedItemA.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )
        XCTAssertEqual(retrievedItemA.isDownloaded, false)
        XCTAssertEqual(retrievedItemA.isUploaded, true)
    }

    func testEnumerateFile() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        var itemAMetadata = remoteItemA.toItemMetadata(account: Self.account)
        itemAMetadata.downloaded = true

        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemAMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemA.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteItemA.identifier),
            account: Self.account,
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
        XCTAssertEqual(
            Int(retrievedItemAItem.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )
        XCTAssertEqual(retrievedItemAItem.isDownloaded, true)
        XCTAssertEqual(retrievedItemAItem.isUploaded, true)

        let dbItemAMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemA.identifier)
        )
        XCTAssertEqual(dbItemAMetadata.ocId, remoteItemA.identifier)
        XCTAssertEqual(dbItemAMetadata.etag, remoteItemA.versionIdentifier)
        XCTAssertTrue(dbItemAMetadata.downloaded)

        // Check download state is not just always true
        Self.dbManager.addItemMetadata(remoteItemB.toItemMetadata(account: Self.account))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemB.identifier))
        let enumerator2 = Enumerator(
            enumeratedItemIdentifier: .init(remoteItemB.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer2 = MockEnumerationObserver(enumerator: enumerator2)
        try await observer2.enumerateItems()
        XCTAssertEqual(observer2.items.count, 1)

        let dbItemBMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemB.identifier)
        )
        XCTAssertEqual(dbItemBMetadata.ocId, remoteItemB.identifier)
        XCTAssertEqual(dbItemBMetadata.etag, remoteItemB.versionIdentifier)
        XCTAssertFalse(dbItemBMetadata.downloaded)
    }

    func testFolderAndContentsChangeEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        remoteFolder.children.removeAll(where: { $0.identifier == remoteItemB.identifier })
        remoteFolder.children.append(remoteItemC)
        remoteItemC.parent = remoteFolder

        let oldFolderEtag = "OLD"
        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = oldFolderEtag
        folderMetadata.downloaded = true // Test downloaded state is properly retained

        let oldItemAEtag = "OLD"
        var itemAMetadata = remoteItemA.toItemMetadata(account: Self.account)
        itemAMetadata.etag = oldItemAEtag
        itemAMetadata.downloaded = true // Test downloaded state is properly retained

        let itemBMetadata = remoteItemB.toItemMetadata(account: Self.account)

        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemAMetadata)
        Self.dbManager.addItemMetadata(itemBMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemA.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemB.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
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
            Self.dbManager.itemMetadata(ocId: remoteFolder.identifier)
        )
        let dbItemAMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemA.identifier)
        )
        let dbItemCMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemC.identifier)
        )
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteItemB.identifier))
        XCTAssertEqual(dbFolderMetadata.etag, remoteFolder.versionIdentifier)
        XCTAssertNotEqual(dbFolderMetadata.etag, oldFolderEtag)
        XCTAssertTrue(dbFolderMetadata.downloaded)
        XCTAssertEqual(dbItemAMetadata.etag, remoteItemA.versionIdentifier)
        XCTAssertNotEqual(dbItemAMetadata.etag, oldItemAEtag)
        XCTAssertTrue(dbItemAMetadata.downloaded)
        XCTAssertEqual(dbItemCMetadata.ocId, remoteItemC.identifier)
        XCTAssertEqual(dbItemCMetadata.etag, remoteItemC.versionIdentifier)
        XCTAssertEqual(dbItemCMetadata.fileName, remoteItemC.name)
        XCTAssertEqual(dbItemCMetadata.fileNameView, remoteItemC.name)
        XCTAssertEqual(dbItemCMetadata.serverUrl, remoteFolder.remotePath)
        XCTAssertEqual(dbItemCMetadata.account, Self.account.ncKitAccount)
        XCTAssertEqual(dbItemCMetadata.user, Self.account.username)
        XCTAssertEqual(dbItemCMetadata.userId, Self.account.id)
        XCTAssertEqual(dbItemCMetadata.urlBase, Self.account.serverUrl)
        XCTAssertFalse(dbItemCMetadata.downloaded)

        XCTAssertNotNil(
            Item.storedItem(
                identifier: .init(remoteFolder.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )

        let retrievedItemA = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemA.identifier }
        ))
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(retrievedItemA.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )

        let retrievedItemC = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemC.identifier }
        ))
        XCTAssertEqual(retrievedItemC.itemIdentifier.rawValue, remoteItemC.identifier)
        XCTAssertEqual(retrievedItemC.filename, remoteItemC.name)
        XCTAssertEqual(retrievedItemC.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemC.creationDate, remoteItemC.creationDate)
        XCTAssertEqual(
            Int(retrievedItemC.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemC.modificationDate.timeIntervalSince1970)
        )
    }

    func testFileMoveChangeEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        remoteFolder.children.removeAll(where: { $0.identifier == remoteItemA.identifier })
        rootItem.children.append(remoteItemA)
        remoteItemA.parent = rootItem
        remoteItemA.remotePath = rootItem.remotePath + "/\(remoteItemA.name)"

        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = "OLD"

        let oldEtag = "OLD"
        let oldServerUrl = remoteFolder.remotePath
        let oldName = "oldItemA"
        var itemAMetadata = remoteItemA.toItemMetadata(account: Self.account)
        itemAMetadata.etag = oldEtag
        itemAMetadata.name = oldName
        itemAMetadata.fileName = oldName
        itemAMetadata.fileNameView = oldName
        itemAMetadata.serverUrl = oldServerUrl

        let itemBMetadata = remoteItemB.toItemMetadata(account: Self.account)

        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemAMetadata)
        Self.dbManager.addItemMetadata(itemBMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemA.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemB.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        // rootContainer has changed, folder has changed, itemA has changed
        XCTAssertEqual(observer.changedItems.count, 2) // Not including target (TODO)
        XCTAssertTrue(observer.deletedItemIdentifiers.isEmpty)

        let retrievedItemA = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemA.identifier }
        ))
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(retrievedItemA.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )

        let storedItemA = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteItemA.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        XCTAssertEqual(storedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(storedItemA.filename, remoteItemA.name)
        XCTAssertEqual(storedItemA.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(storedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(storedItemA.contentModificationDate?.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )

        let storedRootItem = Item.rootContainer(
            account: Self.account, remoteInterface: remoteInterface, dbManager: Self.dbManager
        )
        print(storedRootItem.metadata.serverUrl)
        XCTAssertEqual(storedRootItem.childItemCount?.intValue, 3) // All items

        let storedFolder = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteFolder.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        XCTAssertEqual(storedFolder.childItemCount?.intValue, remoteFolder.children.count)
    }

    func testFileLockStateEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        remoteFolder.children.append(remoteItemC)
        remoteItemC.parent = remoteFolder

        remoteItemA.locked = true
        remoteItemA.lockOwner = Self.account.username
        remoteItemA.lockTimeOut = Date.now.advanced(by: 1_000_000_000_000)

        remoteItemB.locked = true
        remoteItemB.lockOwner = "other different account"
        remoteItemB.lockTimeOut = Date.now.advanced(by: 1_000_000_000_000)

        remoteItemC.locked = true
        remoteItemC.lockOwner = "other different account"
        remoteItemC.lockTimeOut = Date.now.advanced(by: -1_000_000_000_000)

        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = "OLD"

        Self.dbManager.addItemMetadata(folderMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 3)

        let dbItemAMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemA.identifier)
        )
        let dbItemBMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemB.identifier)
        )
        let dbItemCMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemC.identifier)
        )

        XCTAssertEqual(dbItemAMetadata.lock, remoteItemA.locked)
        XCTAssertEqual(dbItemAMetadata.lockOwner, remoteItemA.lockOwner)
        XCTAssertEqual(dbItemAMetadata.lockTimeOut, remoteItemA.lockTimeOut)

        XCTAssertEqual(dbItemBMetadata.lock, remoteItemB.locked)
        XCTAssertEqual(dbItemBMetadata.lockOwner, remoteItemB.lockOwner)
        XCTAssertEqual(dbItemBMetadata.lockTimeOut, remoteItemB.lockTimeOut)

        XCTAssertEqual(dbItemCMetadata.lock, remoteItemC.locked)
        XCTAssertEqual(dbItemCMetadata.lockOwner, remoteItemC.lockOwner)
        XCTAssertEqual(dbItemCMetadata.lockTimeOut, remoteItemC.lockTimeOut)

        let storedItemA = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteItemA.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        let storedItemB = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteItemB.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        let storedItemC = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteItemC.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )

        // Should be able to write to files locked by self
        XCTAssertTrue(storedItemA.fileSystemFlags.contains(.userWritable))
        // Should not be able to write to files locked by someone else
        XCTAssertFalse(storedItemB.fileSystemFlags.contains(.userWritable))
        // Should be able to write to files with an expired lock
        XCTAssertTrue(storedItemC.fileSystemFlags.contains(.userWritable))
    }

    // File Provider system will panic if we give it an NSFileProviderItem with an empty filename.
    // Test that we have a fallback to avoid this, even if something catastrophic happens in the
    // server and the file has no filename
    func testEnsureNoEmptyItemNameEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        remoteItemA.name = ""
        remoteItemA.parent = remoteInterface.rootItem
        rootItem.children = [remoteItemA]

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        // rootContainer has changed, itemA has changed
        XCTAssertEqual(observer.changedItems.count, 1)

        let dbItemAMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemA.identifier)
        )
        XCTAssertEqual(dbItemAMetadata.ocId, remoteItemA.identifier)
        XCTAssertEqual(dbItemAMetadata.fileName, remoteItemA.name)

        let storedItemA = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteItemA.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        XCTAssertEqual(storedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertNotEqual(storedItemA.filename, remoteItemA.name)
        XCTAssertFalse(storedItemA.filename.isEmpty)
    }

    func testListenerInvocations() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)
        let listener = MockEnumerationListener()

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            listener: listener
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 1) // Should only get the folder in root

        // Check enumeration actions
        XCTAssertEqual(listener.startActions.count, 1)
        XCTAssertEqual(listener.finishActions.count, 1)
        XCTAssertTrue(listener.errorActions.isEmpty)
        XCTAssertTrue(listener.startActions.first!.value < listener.finishActions.first!.value)
    }

    func testTrashEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 3)

        let storedItemA = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteTrashItemA.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        XCTAssertEqual(storedItemA.itemIdentifier.rawValue, remoteTrashItemA.identifier)
        XCTAssertEqual(storedItemA.filename, remoteTrashItemA.name)
        XCTAssertEqual(storedItemA.documentSize?.int64Value, remoteTrashItemA.size)
        XCTAssertEqual(storedItemA.isDownloaded, false)
        XCTAssertEqual(storedItemA.isUploaded, true)

        let storedItemB = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteTrashItemB.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        XCTAssertEqual(storedItemB.itemIdentifier.rawValue, remoteTrashItemB.identifier)
        XCTAssertEqual(storedItemB.filename, remoteTrashItemB.name)
        XCTAssertEqual(storedItemB.documentSize?.int64Value, remoteTrashItemB.size)
        XCTAssertEqual(storedItemB.isDownloaded, false)
        XCTAssertEqual(storedItemB.isUploaded, true)

        let storedItemC = try XCTUnwrap(
            Item.storedItem(
                identifier: .init(remoteTrashItemC.identifier),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            )
        )
        XCTAssertEqual(storedItemC.itemIdentifier.rawValue, remoteTrashItemC.identifier)
        XCTAssertEqual(storedItemC.filename, remoteTrashItemC.name)
        XCTAssertEqual(storedItemC.documentSize?.int64Value, remoteTrashItemC.size)
        XCTAssertEqual(storedItemC.isDownloaded, false)
        XCTAssertEqual(storedItemC.isUploaded, true)
    }

    func testTrashChangeEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
        rootTrashItem.children = [remoteTrashItemA]
        remoteTrashItemA.parent = rootTrashItem
        remoteTrashItemB.parent = nil
        remoteTrashItemC.parent = nil

        Self.dbManager.addItemMetadata(
            remoteTrashItemA.toNKTrash().toItemMetadata(account: Self.account)
        )
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemA.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 0)
        observer.reset()

        rootTrashItem.children = [remoteTrashItemA, remoteTrashItemB]
        remoteTrashItemB.parent = rootTrashItem
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 1)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemB.identifier))
        observer.reset()

        rootTrashItem.children = [remoteTrashItemB, remoteTrashItemC]
        remoteTrashItemA.parent = nil
        remoteTrashItemC.parent = rootTrashItem
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 1)
        XCTAssertEqual(observer.deletedItemIdentifiers.count, 1)
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemA.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemB.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemC.identifier))
    }

    func testTrashItemEnumerationFailWhenNoTrashInCapabilities() async {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""undelete": true,"##, with: "")

        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        do {
            try await observer.enumerateItems()
            XCTFail("Item enumeration should have failed!")
        } catch let error {
            XCTAssertEqual((error as NSError?)?.code, NSFeatureUnsupportedError)
        }
    }

    func testTrashChangeEnumerationFailWhenNoTrashInCapabilities() async {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""undelete": true,"##, with: "")

        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        do {
            try await observer.enumerateChanges()
            XCTFail("Item enumeration should have failed!")
        } catch let error {
            XCTAssertEqual((error as NSError?)?.code, NSFeatureUnsupportedError)
        }
    }

    func testRemoteLockFilesNotEnumerated() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)

        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem

        let remoteLockFileItem = MockRemoteItem(
            identifier: "lock-file",
            name: "~$lock-file.docx",
            remotePath: Self.account.davFilesUrl + "/" + remoteFolder.name + "/~$lock-file.docx",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children.append(remoteLockFileItem)
        remoteLockFileItem.parent = rootItem

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 1)
        XCTAssertFalse(
            observer.items.contains(where: { $0.itemIdentifier.rawValue == "lock-file" })
        )
    }

    // Tests situation where we are enumerating files and we can no longer find the parent item
    // in the database. So we need to simulate a situation where this takes place.
    func testCorrectEnumerateFileWithMissingParentInDb() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        var itemAMetadata = remoteItemA.toItemMetadata(account: Self.account)
        itemAMetadata.etag = "OLD"

        Self.dbManager.addItemMetadata(itemAMetadata)
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemA.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 2) // Must include the folder that was missing
        XCTAssertTrue(observer.deletedItemIdentifiers.isEmpty)

        let retrievedItemA = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemA.identifier }
        ))
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(retrievedItemA.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )

        let storedItemAMaybe = await Item.storedItem(
            identifier: .init(remoteItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let storedItemA = try XCTUnwrap(storedItemAMaybe)
        XCTAssertEqual(storedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(storedItemA.filename, remoteItemA.name)
        XCTAssertEqual(storedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(storedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(storedItemA.contentModificationDate?.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )
    }
}

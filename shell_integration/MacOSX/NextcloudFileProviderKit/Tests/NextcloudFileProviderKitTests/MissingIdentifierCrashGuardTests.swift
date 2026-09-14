//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
import TestInterface
import XCTest

/// Guards against nextcloud/desktop#10701: metadata rows with an empty ocId become File Provider
/// items with an empty identifier, which crashes the framework with
/// `__FILEPROVIDER_BAD_ITEM_MISSING_IDENTIFIER__` on both create and change enumeration.
final class MissingIdentifierCrashGuardTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    var rootItem: MockRemoteItem!
    var dbManager: FilesDatabaseManager!

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        rootItem = MockRemoteItem.rootItem(account: Self.account)
        dbManager = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
    }

    /// Inserts a metadata row straight into Realm, bypassing `addItemMetadata`, to simulate a
    /// database already poisoned by a previous corrupt write.
    private func insertRaw(_ metadata: SendableItemMetadata) {
        let database = dbManager.ncDatabase()
        try! database.write {
            database.add(RealmItemMetadata(value: metadata), update: .all)
        }
    }

    // Layer 1: the write side must never persist a row keyed by an empty ocId.
    func testAddItemMetadataRejectsEmptyOcId() {
        var metadata = SendableItemMetadata(ocId: "", fileName: "folder", account: Self.account)
        metadata.directory = true

        dbManager.addItemMetadata(metadata)

        XCTAssertNil(dbManager.itemMetadata(ocId: ""))
    }

    // Layer 2: resolving an empty identifier must never return an item, even if a poisoned row
    // matches. This protects the 405 collision path that feeds the framework a colliding item.
    func testStoredItemReturnsNilForEmptyIdentifier() async {
        var poison = SendableItemMetadata(ocId: "", fileName: "folder", account: Self.account)
        poison.directory = true
        insertRaw(poison)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let item = await Item.storedItem(
            identifier: .init(""),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertNil(item)
    }

    // Layer 3: enumeration must skip empty-ocId rows instead of vending an item with an empty
    // identifier to `didUpdate` / `didEnumerate`.
    func testEnumerationSkipsEmptyOcIdMetadata() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var valid = SendableItemMetadata(ocId: "valid-id", fileName: "good.txt", account: Self.account)
        valid.serverUrl = Self.account.davFilesUrl
        var poison = SendableItemMetadata(ocId: "", fileName: "bad.txt", account: Self.account)
        poison.serverUrl = Self.account.davFilesUrl

        let items = try await [valid, poison].toFileProviderItems(
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertFalse(items.contains { $0.itemIdentifier.rawValue.isEmpty })
        XCTAssertEqual(items.count, 1)
        XCTAssertEqual(items.first?.itemIdentifier.rawValue, "valid-id")
    }

    // Producer: depth-1 PROPFIND ingestion writes directly to Realm, bypassing addItemMetadata. An
    // empty-ocId row here is the most likely source of the poison. It must be dropped on write.
    func testDepthOneIngestionDropsEmptyOcIdRows() {
        let folderPath = Self.account.davFilesUrl + "/folder"

        var target = SendableItemMetadata(ocId: "target-dir", fileName: "folder", account: Self.account)
        target.directory = true
        target.serverUrl = Self.account.davFilesUrl
        var valid = SendableItemMetadata(ocId: "child-valid", fileName: "good.txt", account: Self.account)
        valid.serverUrl = folderPath
        var poison = SendableItemMetadata(ocId: "", fileName: "bad.txt", account: Self.account)
        poison.serverUrl = folderPath

        let changeSet = dbManager.depth1ReadUpdateItemMetadatas(
            account: Self.account.ncKitAccount,
            serverUrl: folderPath,
            updatedMetadatas: [target, valid, poison],
            keepExistingDownloadState: false
        )

        XCTAssertNil(dbManager.itemMetadata(ocId: ""))
        XCTAssertNotNil(dbManager.itemMetadata(ocId: "child-valid"))
        XCTAssertFalse((changeSet?.created ?? []).contains { $0.ocId.isEmpty })
    }

    // Self-heal: a database already poisoned by a previous build must shed its empty-ocId row the
    // next time the containing folder is enumerated, not keep it forever.
    func testDepthOneIngestionPurgesExistingEmptyOcIdRow() {
        let folderPath = Self.account.davFilesUrl + "/folder"

        var poison = SendableItemMetadata(ocId: "", fileName: "bad.txt", account: Self.account)
        poison.serverUrl = folderPath
        insertRaw(poison)
        XCTAssertNotNil(dbManager.itemMetadata(ocId: ""))

        var target = SendableItemMetadata(ocId: "target-dir", fileName: "folder", account: Self.account)
        target.directory = true
        target.serverUrl = Self.account.davFilesUrl

        _ = dbManager.depth1ReadUpdateItemMetadatas(
            account: Self.account.ncKitAccount,
            serverUrl: folderPath,
            updatedMetadatas: [target],
            keepExistingDownloadState: false
        )

        XCTAssertNil(dbManager.itemMetadata(ocId: ""))
    }

    // Deletion path: an empty-ocId row that disappears remotely must not be reported to
    // didDeleteItems as an empty identifier.
    func testChangeBatchSkipsEmptyOcIdDeletions() throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)

        let valid = SendableItemMetadata(ocId: "del-valid", fileName: "a.txt", account: Self.account)
        let poison = SendableItemMetadata(ocId: "", fileName: "b.txt", account: Self.account)

        enumerator.completeChangesBatch(
            observer,
            updated: [],
            deleted: [valid, poison],
            anchor: Enumerator.syncAnchor(at: Date(timeIntervalSince1970: 1)),
            moreComing: false,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )

        XCTAssertEqual(observer.deletedItemIdentifiers.map(\.rawValue), ["del-valid"])
    }

    // Create callback: when the server yields an item without an identifier, create must return an
    // error rather than hand the framework an item with an empty identifier.
    func testCreateFolderReturnsErrorWhenServerOcIdEmpty() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.createFolderIdentifierOverride = ""

        var folderMeta = SendableItemMetadata(ocId: "template-id", fileName: "folder", account: Self.account)
        folderMeta.directory = true
        folderMeta.classFile = NKTypeClassFile.directory.rawValue
        folderMeta.serverUrl = Self.account.davFilesUrl

        let template = Item(
            metadata: folderMeta,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )

        let (created, error) = await Item.create(
            basedOn: template,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertNil(created)
        XCTAssertNotNil(error)
        XCTAssertNil(dbManager.itemMetadata(ocId: ""))
    }
}

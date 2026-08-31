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
}

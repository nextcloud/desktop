//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import TestInterface
import XCTest

final class MissingIdentifierCrashGuardTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    var rootItem: MockRemoteItem!
    var dbManager: FilesDatabaseManager!

    override func setUp() {
        super.setUp()
        rootItem = MockRemoteItem.rootItem(account: Self.account)
        dbManager = try! FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
    }

    private func insertRaw(_ metadata: SendableItemMetadata) throws {
        try dbManager.writer.write { db in
            try ItemMetadataRecord(metadata).upsertRow(db)
        }
    }

    func testAddItemMetadataRejectsEmptyOcId() {
        var metadata = SendableItemMetadata(ocId: "", fileName: "folder", account: Self.account)
        metadata.directory = true

        dbManager.addItemMetadata(metadata)

        XCTAssertNil(dbManager.itemMetadata(ocId: ""))
    }

    func testStoredItemReturnsNilForEmptyIdentifier() async throws {
        var poison = SendableItemMetadata(ocId: "", fileName: "folder", account: Self.account)
        poison.directory = true
        try insertRaw(poison)

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
        XCTAssertEqual(items.map(\.itemIdentifier.rawValue), ["valid-id"])
    }

    func testChangeBatchSkipsEmptyOcIdDeletions() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)

        let valid = SendableItemMetadata(ocId: "del-valid", fileName: "a.txt", account: Self.account)
        let poison = SendableItemMetadata(ocId: "", fileName: "b.txt", account: Self.account)

        let delivery = expectation(description: "Change batch delivered")
        observer.beforeFinishEnumeratingChanges = { delivery.fulfill() }
        enumerator.completeChangesBatch(
            observer,
            updated: [],
            deleted: [valid, poison],
            anchor: NSFileProviderSyncAnchor(Data("1".utf8)),
            moreComing: false,
            acknowledgement: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )

        await fulfillment(of: [delivery], timeout: 5)
        XCTAssertEqual(observer.deletedItemIdentifiers.map(\.rawValue), ["del-valid"])
    }

    func testCreateFileReturnsErrorWhenServerOcIdEmpty() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.uploadIdentifierOverride = ""

        let url = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data([1, 2, 3]).write(to: url)
        defer { try? FileManager.default.removeItem(at: url) }

        var fileMeta = SendableItemMetadata(ocId: "template-id", fileName: "f.txt", account: Self.account)
        fileMeta.serverUrl = Self.account.davFilesUrl

        let template = Item(
            metadata: fileMeta,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )

        let (created, error) = await Item.create(
            basedOn: template,
            contents: url,
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

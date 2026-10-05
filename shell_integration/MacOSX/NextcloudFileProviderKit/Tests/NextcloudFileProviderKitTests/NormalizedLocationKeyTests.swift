//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import XCTest

///
/// Guards for the assumption that lets logical-address lookups be answered from an index: every row
/// carries normalized keys, so `hasLocation` never falls back to the unindexed raw columns.
///
final class NormalizedLocationKeyTests: XCTestCase {
    private static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    private var databaseDirectory: URL!

    override func setUp() {
        super.setUp()
        databaseDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("NormalizedLocationKeyTests-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(at: databaseDirectory, withIntermediateDirectories: true)
    }

    private func makeManager() -> FilesDatabaseManager {
        FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: databaseDirectory,
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
    }

    private func metadata(fileName: String, serverUrl: String, ocId: String) -> SendableItemMetadata {
        var metadata = SendableItemMetadata(ocId: ocId, fileName: fileName, account: Self.account)
        metadata.serverUrl = serverUrl
        metadata.uploaded = true
        return metadata
    }

    /// Every write of the raw location columns goes through `updateLocation`, and this asserts
    /// the outcome rather than the route so it still holds for a new write path.
    func testRowsWrittenThroughThePublicPathCarryTheirNormalizedKeys() {
        let manager = makeManager()
        let serverUrl = Self.account.davFilesUrl + "/folder"

        for (ocId, name) in [("a", "one.txt"), ("b", "two.txt"), ("c", "three.txt")] {
            manager.addItemMetadata(metadata(fileName: name, serverUrl: serverUrl, ocId: ocId))
        }

        let unnormalized = manager.allItemMetadatasForTesting().filter { row in
            guard !row.fileName.isEmpty else {
                return false
            }

            guard let keys = manager.normalizedLocationForTesting(ocId: row.ocId) else {
                return true
            }

            return keys.fileName.isEmpty || keys.serverUrl.isEmpty
        }

        XCTAssertTrue(
            unnormalized.isEmpty,
            "A row with empty normalized keys is invisible to every location lookup."
        )
    }

    /// The safety net for a row that lacks its keys anyway, repaired the next time the database
    /// is opened.
    func testOpeningTheDatabaseRepairsARowWithMissingNormalizedKeys() throws {
        let manager = makeManager()
        let serverUrl = Self.account.davFilesUrl + "/folder"

        // Store empty normalized keys deliberately: this is the row shape the fallback used to carry.
        var stranded = SendableItemMetadata.rawRow(ocId: "stranded")
        stranded.account = Self.account.ncKitAccount
        stranded.fileName = "stranded.txt"
        stranded.serverUrl = serverUrl
        stranded.uploaded = true
        try manager.insertForTesting(stranded, normalizedServerUrl: "", normalizedFileName: "")

        XCTAssertNil(
            manager.itemMetadata(account: Self.account.ncKitAccount, locatedAtRemoteUrl: serverUrl + "/" + "stranded.txt"),
            "Precondition: without its normalized keys the row cannot be found by location."
        )

        // Opening the database again runs the repair.
        let reopened = makeManager()

        let repaired = reopened.itemMetadata(account: Self.account.ncKitAccount, locatedAtRemoteUrl: serverUrl + "/" + "stranded.txt")
        XCTAssertEqual(repaired?.ocId, "stranded", "The repair should make the row findable by location again.")
    }

    /// The repair has to be a fixed point, or a row it cannot actually change is rewritten on every
    /// open.
    func testTheRepairDoesNotRewriteARowItCannotChange() throws {
        let manager = makeManager()

        // An empty raw file name normalizes to itself, so an emptiness test selects this row forever.
        var row = SendableItemMetadata.rawRow(ocId: "emptyName")
        row.account = Self.account.ncKitAccount
        row.serverUrl = Self.account.davFilesUrl + "/folder"
        row.uploaded = true
        try manager.insertForTesting(row, normalizedServerUrl: "", normalizedFileName: "")

        // The first pass legitimately fills in the key the empty name left behind.
        manager.repairPersistedLogicalAddresses()

        // Every row the pass rewrites or evicts is counted in its result, so a later pass
        // reporting none left the row alone.
        let laterPass = manager.repairPersistedLogicalAddresses()

        XCTAssertEqual(laterPass.repaired, 0, "A later repair pass modified a row.")
        XCTAssertEqual(laterPass.evicted, 0, "A later repair pass modified a row.")
    }

    /// A drifted row has to be deduplicated by the pass that repairs it, which holds only while
    /// the walk buckets on the keys it computes rather than the ones already stored.
    func testADriftedDuplicateIsRepairedAndEvictedInOnePass() throws {
        let manager = makeManager()
        let serverUrl = Self.account.davFilesUrl + "/folder"

        var older = SendableItemMetadata.rawRow(ocId: "older")
        older.account = Self.account.ncKitAccount
        older.serverUrl = serverUrl
        older.fileName = "dup.txt"
        older.syncTime = Date(timeIntervalSince1970: 1000)
        older.uploaded = true
        // The row a client older than the migration left behind, raw columns only.
        try manager.insertForTesting(older, normalizedServerUrl: "", normalizedFileName: "")

        var newer = SendableItemMetadata.rawRow(ocId: "newer")
        newer.account = Self.account.ncKitAccount
        newer.serverUrl = serverUrl
        newer.fileName = "dup.txt"
        newer.syncTime = Date(timeIntervalSince1970: 2000)
        newer.uploaded = true
        try manager.insertForTesting(newer)

        manager.repairPersistedLogicalAddresses()

        let olderRow = try XCTUnwrap(manager.itemMetadata(ocId: "older"))
        let newerRow = try XCTUnwrap(manager.itemMetadata(ocId: "newer"))
        let olderKeys = try XCTUnwrap(manager.normalizedLocationForTesting(ocId: "older"))

        XCTAssertEqual(olderKeys.fileName, "dup.txt", "The drifted row should have been repaired.")
        XCTAssertTrue(olderRow.deleted, "The older row at the address should have been evicted.")
        XCTAssertFalse(newerRow.deleted, "The newest settled row at the address should survive.")
    }

    /// The repair covers the rows deduplication excludes, because a tombstone or a lock file is
    /// still resolved by path.
    func testARowExcludedFromDeduplicationIsStillRepaired() throws {
        let manager = makeManager()

        var tombstone = SendableItemMetadata.rawRow(ocId: "tombstone")
        tombstone.account = Self.account.ncKitAccount
        tombstone.serverUrl = Self.account.davFilesUrl + "/folder"
        tombstone.fileName = "gone.txt"
        tombstone.deleted = true
        try manager.insertForTesting(tombstone, normalizedServerUrl: "", normalizedFileName: "")

        manager.repairPersistedLogicalAddresses()

        let repaired = try XCTUnwrap(manager.normalizedLocationForTesting(ocId: "tombstone"))

        XCTAssertEqual(repaired.fileName, "gone.txt")
        XCTAssertEqual(repaired.serverUrl, Self.account.davFilesUrl + "/folder")
    }

    /// Normalization is why the comparison can be an equality at all, since a decomposed name
    /// has to match the precomposed row already stored.
    func testADecomposedNameMatchesThePrecomposedRow() {
        let manager = makeManager()
        let serverUrl = Self.account.davFilesUrl + "/folder"
        // Written as scalars so the file's own encoding cannot quietly normalize the literals.
        let precomposed = "Gr\u{00FC}\u{00DF}e.txt"
        let decomposed = "Gru\u{0308}\u{00DF}e.txt"

        // Swift's own `==` normalizes and reports these as equal, which is why the normalized
        // keys have to exist for the database's byte comparison.
        XCTAssertNotEqual(
            Array(precomposed.utf8), Array(decomposed.utf8),
            "Precondition: the two forms differ in the bytes the database compares."
        )

        manager.addItemMetadata(metadata(fileName: precomposed, serverUrl: serverUrl, ocId: "umlaut"))

        let found = manager.itemMetadata(account: Self.account.ncKitAccount, locatedAtRemoteUrl: serverUrl + "/" + decomposed)

        XCTAssertEqual(found?.ocId, "umlaut", "Lookup must match across Unicode normalization forms.")
    }

    /// A rename rewrites the location, so it has to rewrite the keys with it.
    func testARenameMovesTheNormalizedKeysWithTheRow() {
        let manager = makeManager()
        let serverUrl = Self.account.davFilesUrl + "/folder"
        manager.addItemMetadata(metadata(fileName: "before.txt", serverUrl: serverUrl, ocId: "renamed"))

        manager.renameItemMetadata(ocId: "renamed", newServerUrl: serverUrl, newFileName: "after.txt")

        XCTAssertNil(
            manager.itemMetadata(account: Self.account.ncKitAccount, locatedAtRemoteUrl: serverUrl + "/" + "before.txt"),
            "The old address should no longer resolve."
        )
        XCTAssertEqual(
            manager.itemMetadata(account: Self.account.ncKitAccount, locatedAtRemoteUrl: serverUrl + "/" + "after.txt")?.ocId,
            "renamed",
            "The new address should resolve to the same row."
        )
    }
}

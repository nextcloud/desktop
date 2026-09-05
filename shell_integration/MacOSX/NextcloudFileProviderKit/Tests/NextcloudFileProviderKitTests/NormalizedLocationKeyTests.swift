//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
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
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
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

        let unnormalized = manager.itemMetadatas.filter {
            !$0.fileName.isEmpty && ($0.normalizedFileName.isEmpty || $0.normalizedServerUrl.isEmpty)
        }

        XCTAssertTrue(
            unnormalized.isEmpty,
            "A row with empty normalized keys is invisible to every location lookup."
        )
    }

    /// The safety net for a row that lacks its keys anyway, repaired at open for the cost of one
    /// indexed equality against an empty result set.
    func testOpeningTheDatabaseRepairsARowWithMissingNormalizedKeys() throws {
        let manager = makeManager()
        let serverUrl = Self.account.davFilesUrl + "/folder"
        let database = manager.ncDatabase()

        // Bypass `updateLocation` deliberately: this is the row shape the fallback used to carry.
        try database.write {
            let stranded = RealmItemMetadata()
            stranded.ocId = "stranded"
            stranded.account = Self.account.ncKitAccount
            stranded.fileName = "stranded.txt"
            stranded.serverUrl = serverUrl
            stranded.uploaded = true
            database.add(stranded, update: .all)
        }

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
        let database = manager.ncDatabase()

        // An empty raw file name normalizes to itself, so an emptiness test selects this row forever.
        try database.write {
            let row = RealmItemMetadata()
            row.ocId = "emptyName"
            row.account = Self.account.ncKitAccount
            row.serverUrl = Self.account.davFilesUrl + "/folder"
            row.uploaded = true
            database.add(row, update: .all)
        }

        // The first pass legitimately fills in the key the empty name left behind.
        manager.repairDriftedNormalizedLocationKeys()

        let rewritten = expectation(description: "A later repair pass modified a row.")
        rewritten.isInverted = true
        let token = database.objects(RealmItemMetadata.self).observe { change in
            if case let .update(_, _, _, modifications) = change, !modifications.isEmpty {
                rewritten.fulfill()
            }
        }
        defer { token.invalidate() }

        manager.repairDriftedNormalizedLocationKeys()

        // A rewrite of the row would be notified on the first run-loop turn after the pass
        // returns, so waiting longer buys nothing but suite time.
        wait(for: [rewritten], timeout: 0.2)
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
        // keys have to exist for Realm's byte comparison.
        XCTAssertNotEqual(
            Array(precomposed.utf8), Array(decomposed.utf8),
            "Precondition: the two forms differ in the bytes Realm would compare."
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

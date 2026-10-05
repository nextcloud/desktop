//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

///
/// Coverage for the downgrade guard: what happens when a build meets a database written by another build.
///
/// Serialized because the tests share one defaults suite which they clear.
///
@Suite("Store version guard", .serialized)
struct StoreVersionGuardTests {
    static let suiteName = "com.nextcloud.NextcloudFileProviderKitTests.StoreVersionGuardTests"

    let defaults: UserDefaults
    let directory: URL
    let domain = NSFileProviderDomainIdentifier("guarded-\(UUID().uuidString)")

    init() throws {
        defaults = try #require(UserDefaults(suiteName: Self.suiteName))
        defaults.removePersistentDomain(forName: Self.suiteName)
        directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("StoreVersionGuardTests-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
    }

    var databaseURL: URL {
        directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension)
    }

    var domainDefaults: FileProviderDomainDefaults {
        FileProviderDomainDefaults(identifier: domain, log: FileProviderLogMock(), defaults: defaults)
    }

    func openManager() -> FilesDatabaseManager {
        FilesDatabaseManager(account: DatabaseTestSuites.account, databaseDirectory: directory, fileProviderDomainIdentifier: domain, log: FileProviderLogMock(), defaults: defaults)
    }

    @Test(arguments: [
        (nil, 1, StoreVersionGuard.Decision.fresh),
        (0, 1, .fresh),
        (1, 1, .current),
        (1, 2, .upgrade(from: 1)),
        (2, 1, .downgrade(from: 2)),
        (Int.max, 1, .downgrade(from: Int.max))
    ])
    func decisionTable(seen: Int?, current: Int, expected: StoreVersionGuard.Decision) {
        #expect(StoreVersionGuard.decide(seen: seen, current: current) == expected)
    }

    @Test func openingRecordsTheCurrentVersionInDefaultsAndFile() {
        #expect(domainDefaults.latestSeenDatabaseVersion == nil)
        #expect(StoreVersionGuard.recordedVersion(at: databaseURL) == nil)

        _ = openManager()

        #expect(domainDefaults.latestSeenDatabaseVersion == StoreVersion.current)
        #expect(StoreVersionGuard.recordedVersion(at: databaseURL) == StoreVersion.current)
    }

    @Test func aNewerVersionInTheDefaultsResetsTheStoreAndLowersTheMarker() {
        let earlier = openManager()
        earlier.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "from-the-future", fileName: "future.txt"))
        var marker = domainDefaults
        marker.latestSeenDatabaseVersion = StoreVersion.current + 1

        let manager = openManager()

        #expect(manager.itemMetadata(ocId: "from-the-future") == nil)
        #expect(domainDefaults.latestSeenDatabaseVersion == StoreVersion.current)
        manager.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "fresh", fileName: "fresh.txt"))
        #expect(manager.itemMetadata(ocId: "fresh") != nil)
    }

    @Test func aNewerVersionInTheFileAloneResetsTheStore() throws {
        let earlier = openManager()
        earlier.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "from-the-future", fileName: "future.txt"))
        try earlier.writer.write { db in
            try db.execute(sql: "PRAGMA user_version = \(StoreVersion.current + 1)")
        }
        // The defaults were lost, as after a container reset; the file still tells.
        defaults.removePersistentDomain(forName: Self.suiteName)
        #expect(StoreVersionGuard.recordedVersion(at: databaseURL) == StoreVersion.current + 1)

        let manager = openManager()

        #expect(manager.itemMetadata(ocId: "from-the-future") == nil)
        #expect(StoreVersionGuard.recordedVersion(at: databaseURL) == StoreVersion.current)
    }

    @Test func unknownMigrationsInTheFileCountAsNewer() throws {
        let earlier = openManager()
        try earlier.writer.write { db in
            try db.execute(sql: "INSERT INTO grdb_migrations (identifier) VALUES ('v99_from_the_future')")
        }

        #expect(StoreVersionGuard.recordedVersion(at: databaseURL) == Int.max)
    }

    @Test func theCurrentVersionKeepsTheRows() {
        let earlier = openManager()
        earlier.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "kept", fileName: "kept.txt"))

        #expect(openManager().itemMetadata(ocId: "kept") != nil)
        #expect(domainDefaults.latestSeenDatabaseVersion == StoreVersion.current)
    }

    @Test func theDefaultsRoundTripTheMarker() {
        var marker = domainDefaults
        #expect(marker.latestSeenDatabaseVersion == nil)

        marker.latestSeenDatabaseVersion = 3
        #expect(domainDefaults.latestSeenDatabaseVersion == 3)

        marker.latestSeenDatabaseVersion = nil
        #expect(domainDefaults.latestSeenDatabaseVersion == nil)
    }
}

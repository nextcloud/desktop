//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

///
/// Coverage for the store version guard: what happens when a build meets a database written by another build, newer or older.
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
        try! FilesDatabaseManager(account: DatabaseTestSuites.account, databaseDirectory: directory, fileProviderDomainIdentifier: domain, log: FileProviderLogMock(), defaults: defaults)
    }

    @Test(arguments: [
        (nil, 1, 1, StoreVersionGuard.Decision.fresh),
        (0, 1, 1, .fresh),
        (1, 1, 1, .current),
        (1, 2, 1, .upgrade(from: 1)),
        (2, 1, 1, .downgrade(from: 2)),
        (Int.max, 1, 1, .downgrade(from: Int.max)),
        (1, 3, 2, .obsolete(from: 1)),
        (2, 3, 2, .upgrade(from: 2)),
        (1, 2, 2, .obsolete(from: 1)),
        (nil, 2, 2, .fresh),
        (2, 2, 2, .current)
    ])
    func decisionTable(seen: Int?, current: Int, oldestMigratable: Int, expected: StoreVersionGuard.Decision) {
        #expect(StoreVersionGuard.decide(seen: seen, current: current, oldestMigratable: oldestMigratable) == expected)
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
        #expect(manager.storeIdentity != earlier.storeIdentity)
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
        earlier.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "dropped", fileName: "dropped.txt"))
        try earlier.writer.write { db in
            try db.execute(sql: "INSERT INTO grdb_migrations (identifier) VALUES ('v99_from_the_future')")
        }
        earlier.checkpointForShutdown()

        #expect(StoreVersionGuard.recordedVersion(at: databaseURL) == Int.max)

        let manager = openManager()

        let migrations = try manager.writer.read { db in
            try String.fetchAll(db, sql: "SELECT identifier FROM grdb_migrations ORDER BY identifier")
        }
        #expect(manager.itemMetadata(ocId: "dropped") == nil)
        #expect(manager.storeIdentity != earlier.storeIdentity)
        #expect(migrations == ["v1_initial"])
    }

    @Test func theOldestMigratableVersionIsNotAheadOfTheCurrentOne() {
        #expect(StoreVersion.oldestMigratable <= StoreVersion.current, "Otherwise every open sets the store up from scratch again.")
    }

    @Test func aStoreWhichCannotBeRemovedIsNotOpened() throws {
        // The open pool keeps the journal files in place, so SQLite could still write to the file once nothing in the directory can be removed.
        let pool = try DatabasePool(path: databaseURL.path)
        try DatabaseSchema.migrator.migrate(pool)
        try pool.write { db in
            try db.execute(sql: "PRAGMA user_version = \(StoreVersion.current + 1)")
        }
        try FileManager.default.setAttributes([.posixPermissions: 0o555], ofItemAtPath: directory.path)
        defer { try? FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: directory.path) }

        #expect(throws: FilesDatabaseManager.OpenError.self) {
            try FilesDatabaseManager(account: DatabaseTestSuites.account, databaseDirectory: directory, fileProviderDomainIdentifier: domain, log: FileProviderLogMock(), defaults: defaults)
        }
        #expect(StoreVersionGuard.recordedVersion(at: databaseURL) == StoreVersion.current + 1, "The file this build cannot use is left as it was, not stamped with this build's version.")
        #expect(domainDefaults.latestSeenDatabaseVersion == nil)
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

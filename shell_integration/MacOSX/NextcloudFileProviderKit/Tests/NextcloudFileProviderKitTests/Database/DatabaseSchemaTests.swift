//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for the schema the migrator creates and the query plans it supports.
    ///
    @Suite("Schema")
    struct DatabaseSchemaTests {
        let manager = DatabaseTestSuites.makeManager()

        @Test func migratorCreatesTheFinalSchemaInOneStep() throws {
            try manager.writer.read { db in
                let tables = try DatabaseSchema.tableNames.map { try db.tableExists($0) }
                let itemColumns = try db.columns(in: ItemMetadataRecord.databaseTableName).map(\.name)
                let identityColumns = try db.columns(in: StoreIdentityRecord.databaseTableName).map(\.name)
                #expect(!tables.contains(false))
                #expect(DatabaseSchema.tableNames.count == 7)
                #expect(try DatabaseSchema.migrator.appliedIdentifiers(db) == ["v1_metadata"])
                #expect(!itemColumns.contains("path"))
                #expect(identityColumns.contains("accountBinding"))
            }
        }

        @Test func reopeningAnExistingDatabaseKeepsItsRows() throws {
            let directory = try #require(manager.databaseURL).deletingLastPathComponent()
            let domain = try #require(manager.databaseURL).deletingPathExtension().lastPathComponent
            manager.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "kept", fileName: "kept.txt"))

            let reopened = try FilesDatabaseManager(
                account: DatabaseTestSuites.account,
                databaseDirectory: directory,
                fileProviderDomainIdentifier: .init(domain),
                log: FileProviderLogMock()
            )

            #expect(reopened.itemMetadata(ocId: "kept")?.fileName == "kept.txt")
            #expect(reopened.storeIdentity == manager.storeIdentity)
            let version = try reopened.writer.read { db in try Int.fetchOne(db, sql: "PRAGMA user_version") }
            #expect(version == StoreVersion.current)
        }

        @Test func anUnreadableItemDoesNotPreventFillingMissingAccountDetails() throws {
            let directory = try #require(manager.databaseURL).deletingLastPathComponent()
            let domain = UUID().uuidString
            let url = directory.appendingPathComponent(domain).appendingPathExtension(DatabaseSchema.fileExtension)
            func itemMissingAccountDetails(ocId: String, fileName: String) -> SendableItemMetadata {
                var item = DatabaseTestSuites.makeFile(ocId: ocId, fileName: fileName)
                item.account = ""
                item.urlBase = ""
                item.userId = ""
                return item
            }
            let healthy = itemMissingAccountDetails(ocId: "healthy", fileName: "healthy.txt")
            let damagedName = itemMissingAccountDetails(ocId: "damaged-name", fileName: "old-name.txt")
            let damagedID = itemMissingAccountDetails(ocId: "damaged-id", fileName: "old-id.txt")
            let invalidUTF8 = Data([0xff])
            _ = try FilesDatabaseManager.writeDatabaseWithoutSavedDAVAccountForTesting(
                at: url,
                metadata: [healthy, damagedName, damagedID]
            )

            let legacy = try DatabaseQueue(path: url.path)
            try legacy.write { db in
                try db.execute(
                    sql: "UPDATE itemMetadata SET fileName = ? WHERE ocId = ?",
                    arguments: [invalidUTF8, damagedName.ocId]
                )
                try db.execute(
                    sql: "UPDATE itemMetadata SET ocId = CAST(? AS TEXT) WHERE ocId = ?",
                    arguments: [invalidUTF8, damagedID.ocId]
                )
            }

            let reopened = try FilesDatabaseManager(
                account: DatabaseTestSuites.account,
                databaseDirectory: directory,
                fileProviderDomainIdentifier: .init(domain),
                log: FileProviderLogMock()
            )

            #expect(reopened.itemMetadata(locatedAtRemoteUrl: healthy.remotePath())?.ocId == healthy.ocId)
            #expect(try reopened.storedParentLocationKeysForTesting(ocId: healthy.ocId)?.parentLocation == "files:")
        }

        @Test func aCorruptDatabaseFileIsSetAsideAndReplaced() throws {
            let url = try #require(manager.databaseURL)
            let directory = url.deletingLastPathComponent()
            let domain = url.deletingPathExtension().lastPathComponent
            manager.checkpointForShutdown()
            try Data((0 ..< 8192).map { _ in UInt8.random(in: 0 ... 255) }).write(to: url)

            let reopened = try FilesDatabaseManager(
                account: DatabaseTestSuites.account,
                databaseDirectory: directory,
                fileProviderDomainIdentifier: .init(domain),
                log: FileProviderLogMock()
            )

            reopened.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "fresh", fileName: "fresh.txt"))
            #expect(reopened.itemMetadata(ocId: "fresh") != nil)
            #expect(reopened.writer is DatabasePool, "The replacement is a file on disk, not an in-memory fallback.")
            #expect(reopened.storeIdentity != manager.storeIdentity, "The replacement expires every anchor of the old file.")
            let setAside = try FileManager.default.contentsOfDirectory(atPath: directory.path).filter { $0.contains(".unreadable-") }
            #expect(!setAside.isEmpty)
        }

        @Test func openingInADirectoryNothingCanBeWrittenToThrows() throws {
            let directory = FileManager.default.temporaryDirectory
                .appendingPathComponent("DatabaseSchemaTests-readonly-\(UUID().uuidString)", isDirectory: true)
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            try FileManager.default.setAttributes([.posixPermissions: 0o555], ofItemAtPath: directory.path)
            defer { try? FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: directory.path) }

            #expect(throws: FilesDatabaseManager.OpenError.self) {
                try FilesDatabaseManager(
                    account: DatabaseTestSuites.account,
                    databaseDirectory: directory,
                    fileProviderDomainIdentifier: .init("readonly"),
                    log: FileProviderLogMock(),
                    defaults: DatabaseTestSuites.defaults
                )
            }
            #expect(FileManager.default.fileExists(atPath: directory.appendingPathComponent("readonly.sqlite").path) == false)
        }

        @Test func fileIdLookupsUseTheFileIdIndex() throws {
            let plan = try manager.writer.read { db in
                let statement = try ItemMetadataRecord
                    .filter(["1", "2", "3"].contains(ItemMetadataRecord.Columns.fileId))
                    .makePreparedRequest(db).statement
                return try Row
                    .fetchAll(db, sql: "EXPLAIN QUERY PLAN " + statement.sql, arguments: statement.arguments)
                    .map { $0["detail"] as String }
                    .joined(separator: " | ")
            }

            #expect(plan.contains("itemMetadata_on_fileId"), "Plan: \(plan)")
            #expect(!plan.contains("SCAN itemMetadata"), "Plan: \(plan)")
        }

        @Test func writesSetTheRequestedDurabilityOnTheWriter() throws {
            let row = DatabaseTestSuites.makeFile(ocId: "pinned", fileName: "pinned.txt")

            manager.addItemMetadata(row)
            #expect(try manager.synchronousLevelForTesting() == 2, "Local writes commit with FULL.")

            manager.addItemMetadataPreservingLocalState(row)
            #expect(try manager.synchronousLevelForTesting() == 1, "Server-derived bulk writes commit with NORMAL.")

            _ = try manager.set(keepDownloaded: true, for: row)
            #expect(try manager.synchronousLevelForTesting() == 2)
            #expect(manager.itemMetadata(ocId: "pinned")?.keepDownloaded == true)
        }

        @Test func locationQueriesUseTheLocationIndex() throws {
            let plans = try manager.writer.read { db in
                try [
                    ItemMetadataRecord.filter(ItemMetadataRecord.matchesStoredParentLocationAndFileName(parentLocation: "https://a/b", fileName: "c")),
                    ItemMetadataRecord.filter(ItemMetadataRecord.matchesStoredParentLocation("https://a/b", includingDescendants: false)),
                    ItemMetadataRecord.filter(ItemMetadataRecord.matchesStoredParentLocation("https://a/b", includingDescendants: true))
                ].map { request -> String in
                    let statement = try request.makePreparedRequest(db).statement
                    return try Row
                        .fetchAll(db, sql: "EXPLAIN QUERY PLAN " + statement.sql, arguments: statement.arguments)
                        .map { $0["detail"] as String }
                        .joined(separator: " | ")
                }
            }

            for plan in plans {
                #expect(plan.contains("itemMetadata_on_location"), "Plan: \(plan)")
                #expect(!plan.contains("SCAN itemMetadata"), "Plan: \(plan)")
            }
        }
    }
}

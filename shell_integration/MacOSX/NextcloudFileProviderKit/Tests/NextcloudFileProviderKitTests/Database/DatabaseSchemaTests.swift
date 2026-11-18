//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import CryptoKit
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

        @Test func migratorCreatesEveryTable() throws {
            let tables = try manager.writer.read { db in
                try DatabaseSchema.tableNames.map { try db.tableExists($0) }
            }

            #expect(!tables.contains(false))
            #expect(DatabaseSchema.tableNames.count == 7)
        }

        @Test func theSchemaIsPinnedToItsStoreVersion() throws {
            let schema = try manager.writer.read { db in
                var lines: [String] = []

                for table in try String.fetchAll(db, sql: "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' AND name NOT LIKE 'grdb_%' ORDER BY name") {
                    for column in try db.columns(in: table) {
                        lines.append("\(table).\(column.name) \(column.type) notNull=\(column.isNotNull) default=\(column.defaultValueSQL ?? "-") primaryKey=\(column.primaryKeyIndex)")
                    }

                    for index in try db.indexes(on: table).sorted(by: { $0.name < $1.name }) {
                        lines.append("\(table) index \(index.name) on \(index.columns) unique=\(index.isUnique)")
                    }
                }

                return lines.joined(separator: "\n")
            }
            let digest = SHA256.hash(data: Data(schema.utf8)).map { String(format: "%02x", $0) }.joined()

            // A change of the schema changes this digest. Raise StoreVersion.current with it and, as StoreVersion describes, StoreVersion.oldestMigratable too; then update all three expectations.
            #expect(digest == "80859b3aee2988f234c07e0c003e0e8535cb8c11bc422800d8eaf795a94bc309", "Schema:\n\(schema)")
            #expect(StoreVersion.current == 2)
            #expect(StoreVersion.oldestMigratable == 2)
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

        @Test func theReportedSyncTimeColumnIsNullableAndOutsideTheRecord() throws {
            let column = try manager.writer.read { db in
                try db.columns(in: ItemMetadataRecord.databaseTableName).first { $0.name == "reportedSyncTime" }
            }

            #expect(try #require(column).type == "REAL")
            #expect(try #require(column).isNotNull == false)
            #expect(!ItemMetadataRecord.Columns.allCases.map(\.rawValue).contains("reportedSyncTime"), "The record's upsert must not reset the mark.")
        }

        @Test func markingUsesThePrimaryKey() throws {
            let plan = try manager.writer.read { db in
                try Row
                    .fetchAll(db, sql: "EXPLAIN QUERY PLAN " + FilesDatabaseManager.markReportedSQL, arguments: [1.0, "x", 1.0])
                    .map { $0["detail"] as String }
                    .joined(separator: " | ")
            }

            #expect(plan.contains("sqlite_autoindex_itemMetadata_1"), "Plan: \(plan)")
            #expect(!plan.contains("SCAN itemMetadata"), "Plan: \(plan)")
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
                    ItemMetadataRecord.filter(ItemMetadataRecord.hasLocation(serverUrl: "https://a/b", fileName: "c")),
                    ItemMetadataRecord.filter(ItemMetadataRecord.hasServerUrl(equalTo: "https://a/b", includingDescendants: false)),
                    ItemMetadataRecord.filter(ItemMetadataRecord.hasServerUrl(equalTo: "https://a/b", includingDescendants: true))
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

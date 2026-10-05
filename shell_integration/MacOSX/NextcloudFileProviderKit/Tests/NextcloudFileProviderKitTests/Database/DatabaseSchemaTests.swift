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

        @Test func migratorCreatesEveryTable() throws {
            let tables = try manager.writer.read { db in
                try DatabaseSchema.tableNames.map { try db.tableExists($0) }
            }

            #expect(!tables.contains(false))
            #expect(DatabaseSchema.tableNames.count == 6)
        }

        @Test func reopeningAnExistingDatabaseKeepsItsRows() throws {
            let directory = try #require(manager.databaseURL).deletingLastPathComponent()
            let domain = try #require(manager.databaseURL).deletingPathExtension().lastPathComponent
            manager.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "kept", fileName: "kept.txt"))

            let reopened = FilesDatabaseManager(
                account: DatabaseTestSuites.account,
                databaseDirectory: directory,
                fileProviderDomainIdentifier: .init(domain),
                log: FileProviderLogMock()
            )

            #expect(reopened.itemMetadata(ocId: "kept")?.fileName == "kept.txt")
            let version = try reopened.writer.read { db in try Int.fetchOne(db, sql: "PRAGMA user_version") }
            #expect(version == StoreVersion.current)
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

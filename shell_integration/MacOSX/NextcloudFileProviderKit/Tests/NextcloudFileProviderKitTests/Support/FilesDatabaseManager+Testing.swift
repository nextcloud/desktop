//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB
@testable import NextcloudFileProviderKit
import Testing

///
/// Direct access to stored rows for tests.
///
/// This is the only place in the test target which touches the storage engine. Tests seed and inspect rows through these methods so that an engine change touches one file.
///
extension FilesDatabaseManager {
    ///
    /// Store a row exactly as given, bypassing duplicate eviction and the inheritance of local flags.
    ///
    /// The normalized location keys are derived from the raw location unless overridden, which builds rows whose keys drifted.
    ///
    func insertForTesting(_ metadata: SendableItemMetadata, normalizedServerUrl: String? = nil, normalizedFileName: String? = nil) throws {
        var record = ItemMetadataRecord(metadata)

        if let normalizedServerUrl {
            record.normalizedServerUrl = normalizedServerUrl
        }

        if let normalizedFileName {
            record.normalizedFileName = normalizedFileName
        }

        try writer.write { db in
            try record.upsertRow(db)
        }
    }

    ///
    /// The stored normalized location keys of a row.
    ///
    func normalizedLocationForTesting(ocId: String) -> (serverUrl: String, fileName: String)? {
        let record = try? writer.read { db in
            try ItemMetadataRecord.fetchOne(db, key: ocId)
        }

        guard let record = record ?? nil else {
            return nil
        }

        return (record.normalizedServerUrl, record.normalizedFileName)
    }

    ///
    /// Every stored item row, ordered by identifier.
    ///
    func allItemMetadatasForTesting() -> [SendableItemMetadata] {
        let rows = try? writer.read { db in
            try ItemMetadataRecord.order(ItemMetadataRecord.Columns.ocId).fetchAll(db)
        }

        return (rows ?? []).map(\.metadata)
    }

    ///
    /// Drop a table through a second connection so that every later statement touching it fails, which is how tests provoke a read or write failure on an open manager.
    ///
    func breakTableForTesting(_ tableName: String) throws {
        let path = try #require(databaseURL).path
        let queue = try DatabaseQueue(path: path)
        try queue.write { db in
            try db.execute(sql: "DROP TABLE \(tableName)")
        }
    }

    ///
    /// Recreate the schema after ``breakTableForTesting(_:)`` on a manager shared by several tests.
    ///
    func recreateTablesForTesting() {
        try? writer.write { db in
            for table in DatabaseSchema.tableNames where try db.tableExists(table) {
                try db.execute(sql: "DROP TABLE \(table)")
            }
            try db.execute(sql: "DELETE FROM grdb_migrations")
        }
        try? DatabaseSchema.migrator.migrate(writer)
    }

    ///
    /// Remove every row of every table.
    ///
    func removeAllRowsForTesting() throws {
        try writer.write { db in
            for table in DatabaseSchema.tableNames {
                try db.execute(sql: "DELETE FROM \(table)")
            }
        }
    }
}

extension SendableItemMetadata {
    ///
    /// A row carrying only the storage defaults: empty strings, `false`, zero and the current date.
    ///
    /// Use it where a test used to build a bare database object and set a handful of fields, so the untouched fields keep the values the engine would have given them.
    ///
    static func rawRow(ocId: String) -> SendableItemMetadata {
        SendableItemMetadata(
            ocId: ocId,
            account: "",
            classFile: "",
            contentType: "",
            creationDate: Date(),
            directory: false,
            e2eEncrypted: false,
            etag: "",
            fileId: "",
            fileName: "",
            fileNameView: "",
            ownerId: "",
            ownerDisplayName: "",
            path: "",
            permissions: "",
            serverUrl: "",
            size: 0,
            urlBase: "",
            user: "",
            userId: ""
        )
    }
}

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
    /// Seed the current schema before startup saves the DAV account in SQLite.
    /// This is the SQLite state produced by the Realm import's item row conversion.
    static func writeDatabaseWithoutSavedDAVAccountForTesting(
        at url: URL,
        metadata: [SendableItemMetadata]
    ) throws -> String {
        try writeDatabaseForTesting(at: url, metadata: metadata)
    }

    /// Seed SQLite as it looked after an earlier startup, with the saved account
    /// and item metadata supplied independently.
    static func writeDatabaseForTesting(
        at url: URL,
        metadata: [SendableItemMetadata],
        boundTo account: Account? = nil
    ) throws -> String {
        let queue = try DatabaseQueue(path: url.path)
        try DatabaseSchema.migrator.migrate(queue)

        return try queue.write { db in
            try db.execute(sql: "PRAGMA user_version = \(StoreVersion.current)")
            for item in metadata {
                try ItemMetadataRecord(item).upsertRow(db)
            }
            var store = try StoreIdentityRecord.fetchOne(db) ?? StoreIdentityRecord(identity: UUID().uuidString)
            if let account {
                store.accountBinding = try JSONEncoder().encode(account.davAccountIdentity.serializedFields)
            }
            try store.upsert(db)
            return store.identity
        }
    }

    func storedParentLocationKeysForTesting(ocId: String) throws -> (parentLocation: String, normalizedParentLocation: String)? {
        try writer.read { db in
            try Row.fetchOne(
                db,
                sql: "SELECT serverUrl, normalizedServerUrl FROM itemMetadata WHERE ocId = ?",
                arguments: [ocId]
            ).map { ($0["serverUrl"], $0["normalizedServerUrl"]) }
        }
    }

    /// Seed a pending change alongside the imported item before startup saves the DAV account in SQLite.
    static func writeDatabaseWithQueuedUpdateAndNoSavedDAVAccountForTesting(
        at url: URL,
        metadata: SendableItemMetadata,
        sessionId: String
    ) throws -> String {
        let identity = try writeDatabaseWithoutSavedDAVAccountForTesting(at: url, metadata: [metadata])
        let queue = try DatabaseQueue(path: url.path)
        try queue.write { db in
            try ChangeDeliverySessionRecord(
                sessionId: sessionId,
                containerKey: "working-set",
                currentAnchorKey: "old-anchor",
                finalAnchorRawValue: Data(),
                incomplete: false,
                hardRemoveDeleted: false
            ).insert(db)
            try ChangeDeliveryItemRecord(
                sessionId: sessionId,
                sequence: 0,
                metadataData: try ChangeDeliveryItemRecord.encodeMetadata(metadata),
                deleted: false
            ).insert(db)
        }
        return identity
    }

    ///
    /// Store a row exactly as given, bypassing duplicate eviction and the inheritance of local flags.
    ///
    /// The normalized location keys are derived from the raw location unless overridden, which builds rows whose keys drifted.
    ///
    func insertForTesting(_ metadata: SendableItemMetadata, normalizedServerUrl: String? = nil, normalizedFileName: String? = nil) throws {
        var record = ItemMetadataRecord(metadata, domainAccount: account)

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

    /// Reject metadata inserts and updates while keeping reads and change delivery available.
    func setItemMetadataWriteFailureForTesting(_ enabled: Bool) throws {
        try writer.write { db in
            for operation in ["INSERT", "UPDATE"] {
                let trigger = "fail_item_metadata_\(operation)"
                if enabled {
                    try db.execute(sql: """
                    CREATE TRIGGER \(trigger) BEFORE \(operation) ON itemMetadata
                    BEGIN
                        SELECT RAISE(ABORT, 'Injected metadata write failure');
                    END
                    """)
                } else {
                    try db.execute(sql: "DROP TRIGGER IF EXISTS \(trigger)")
                }
            }
        }
    }

    /// Park the writer on a dispatch queue until the test releases it, with a timeout as a backstop.
    func holdWriterForTesting(until release: DispatchSemaphore) async throws {
        let (entered, continuation) = AsyncStream<Result<Void, any Error>>.makeStream()
        DispatchQueue.global().async {
            defer { continuation.finish() }
            do {
                try self.writer.writeWithoutTransaction { _ in
                    continuation.yield(.success(()))
                    _ = release.wait(timeout: .now() + .seconds(Int(asyncTestTimeout.components.seconds) * 2))
                }
            } catch {
                continuation.yield(.failure(error))
            }
        }
        guard let result = try await nextTestValue(from: entered) else {
            throw URLError(.badServerResponse)
        }
        try result.get()
    }

    /// Apply a directory rename after the next write commits.
    func renameDirectoryAfterNextTransactionForTesting(
        ocId: String,
        newServerUrl: String,
        newFileName: String
    ) async throws {
        try await writer.writeWithoutTransaction { db in
            db.afterNextTransaction { db in
                try? db.inTransaction {
                    _ = try self.renameDirectoryAndPropagateToChildren(
                        ocId: ocId,
                        newServerUrl: newServerUrl,
                        newFileName: newFileName,
                        in: db
                    )
                    return .commit
                }
            }
        }
    }

    /// Run the handler inside the transaction after a row is marked downloaded.
    func setDownloadCompletionWriteHandlerForTesting(_ handler: (@Sendable () -> Void)?) throws {
        let function = DatabaseFunction("on_download_completion_write", argumentCount: 0) { _ in
            handler?()
            return nil
        }
        try writer.write { db in
            if handler != nil {
                db.add(function: function)
                try db.execute(sql: """
                CREATE TEMP TRIGGER on_download_completion_write AFTER UPDATE ON itemMetadata
                WHEN NEW.downloaded = 1
                BEGIN
                    SELECT on_download_completion_write();
                END
                """)
            } else {
                try db.execute(sql: "DROP TRIGGER IF EXISTS on_download_completion_write")
                db.remove(function: function)
            }
        }
    }

    /// Run the handler after inserting a specific item, before the transaction commits.
    func setItemMetadataInsertionHandlerForTesting(ocId: String, _ handler: (@Sendable () -> Void)?) throws {
        let function = DatabaseFunction("on_item_metadata_insert", argumentCount: 0) { _ in
            handler?()
            return nil
        }
        try writer.write { db in
            if handler != nil {
                let identifier = ocId.replacingOccurrences(of: "'", with: "''")
                db.add(function: function)
                try db.execute(sql: """
                CREATE TEMP TRIGGER on_item_metadata_insert AFTER INSERT ON itemMetadata
                WHEN NEW.ocId = '\(identifier)'
                BEGIN
                    SELECT on_item_metadata_insert();
                END
                """)
            } else {
                try db.execute(sql: "DROP TRIGGER IF EXISTS on_item_metadata_insert")
                db.remove(function: function)
            }
        }
    }

    /// Reject publishing a completed download while allowing its start and cleanup writes.
    func setDownloadCompletionWriteFailureForTesting(_ enabled: Bool, ocId: String) throws {
        try writer.write { db in
            if enabled {
                let identifier = ocId.replacingOccurrences(of: "'", with: "''")
                try db.execute(sql: """
                CREATE TRIGGER fail_download_completion BEFORE UPDATE ON itemMetadata
                WHEN NEW.ocId = '\(identifier)' AND NEW.downloaded = 1
                BEGIN
                    SELECT RAISE(ABORT, 'Injected download completion write failure');
                END
                """)
            } else {
                try db.execute(sql: "DROP TRIGGER IF EXISTS fail_download_completion")
            }
        }
    }

    ///
    /// Recreate the schema after ``breakTableForTesting(_:)`` on a manager shared by several tests.
    ///
    func recreateTablesForTesting() {
        try? writer.write { db in
            for table in DatabaseSchema.tableNames {
                try db.execute(sql: "DROP TABLE IF EXISTS \(table)")
            }
            try db.execute(sql: "DELETE FROM grdb_migrations")
        }
        try? DatabaseSchema.migrator.migrate(writer)
        try? writer.write { db in
            try StoreIdentityRecord(identity: storeIdentity).insert(db)
        }
    }

    ///
    /// The `PRAGMA synchronous` level left on the writer by the last write: `1` for ``WriteDurability/relaxed``, `2` for ``WriteDurability/full``.
    ///
    func synchronousLevelForTesting() throws -> Int? {
        try writer.writeWithoutTransaction { db in try Int.fetchOne(db, sql: "PRAGMA synchronous") }
    }

    ///
    /// Clear domain data while retaining the database file's identity.
    ///
    func removeAllRowsForTesting() throws {
        try writer.write { db in
            for table in DatabaseSchema.tableNames where table != StoreIdentityRecord.databaseTableName {
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
            permissions: "",
            serverUrl: "",
            size: 0,
            urlBase: "",
            user: "",
            userId: ""
        )
    }
}

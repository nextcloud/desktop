//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB

extension FilesDatabaseManager {
    ///
    /// Soft-delete any other non-deleted rows at the same logical address as
    /// `incoming` whose `ocId` differs.
    ///
    /// `ocId` is the primary key, so an upsert dedupes by `ocId` only. When
    /// the server returns the same logical path with a fresh `ocId`
    /// (restore-from-trash, another client recreating the item during
    /// reconnect, an upload finalizer assigning a server-issued `ocId`), the
    /// upsert inserts a second row beside the first. macOS can only represent
    /// one sibling per logical name and renames the second with a `" 2"`
    /// suffix when this surfaces in Finder.
    ///
    /// Call this inside the caller's transaction.
    ///
    /// In-flight rows (whose ``ItemMetadata/status`` is not ``Status/normal``)
    /// and lock files of local origin are skipped: soft-deleting an
    /// in-flight row would yank the file out from under a pending
    /// `NSURLSession` task, and lock files of local origin must mirror the
    /// existing exclusion in the materialised-deletions code path.
    /// In-flight collisions are logged at `error` level for support
    /// visibility.
    ///
    /// - Parameters:
    ///   - incoming: The metadata about to be persisted; its `(account,
    ///     serverUrl, fileName)` defines the logical address used for the
    ///     collision query, while its `ocId` is excluded from candidates so
    ///     that the normal same-`ocId` upsert is unaffected.
    ///   - db: The open database.
    ///   - now: Timestamp stamped on every evicted row so the change
    ///     surfaces through ``pendingWorkingSetChanges(since:)``.
    ///
    /// - Returns: The `ocId` values of rows that were soft-deleted.
    ///
    @discardableResult
    func evictLogicalDuplicates(of incoming: any ItemMetadata, in db: Database, now: Date = Date()) throws -> [String] {
        // A lock file created by the local OS should never trigger eviction:
        // it is not authoritative about the server-side state at its logical
        // address and could otherwise soft-delete a legitimate server row.
        if incoming.isLockFileOfLocalOrigin {
            return []
        }

        // A soft-deleted incoming row is a tombstone, not an authoritative live
        // occupant of its logical address, so it must never evict a live sibling.
        // A deletion is authoritative only about its own ocId; if another live row
        // shares the (account, serverUrl, fileName) it is the current truth and
        // must survive. Without this, persisting the tombstone of a stale ocId
        // after an app "safe save" (create -> delete -> recreate, which rotates the
        // ocId) soft-deletes the freshly recreated live file. (Ticket 96101301)
        if incoming.deleted {
            return []
        }

        // Prepared once per connection: this runs for every row of a large directory write.
        let candidates = try ItemLogicalAddressRow.fetchAll(
            db.cachedStatement(sql: Self.logicalDuplicateCandidatesSQL),
            arguments: [
                incoming.fileName.precomposedStringWithCanonicalMapping,
                incoming.serverUrl.precomposedStringWithCanonicalMapping,
                incoming.ocId
            ]
        )

        var evicted: [String] = []

        for candidate in candidates {
            if candidate.status != Status.normal.rawValue {
                logger.error("Skipping eviction of in-flight logical duplicate.", [
                    .item: candidate.ocId,
                    .name: candidate.fileName,
                    .url: candidate.serverUrl,
                    .syncTime: candidate.syncTime
                ])

                continue
            }

            try db.cachedStatement(sql: Self.softDeleteSQL).execute(arguments: [now.timeIntervalSinceReferenceDate, candidate.ocId])
            evicted.append(candidate.ocId)

            logger.info("Evicted logical duplicate.", [
                .item: candidate.ocId,
                .name: candidate.fileName,
                .url: candidate.serverUrl,
                .syncTime: now
            ])
        }

        return evicted
    }

    /// Live, non-lock-file rows at a normalized location other than the given identifier, in the columns of ``ItemLogicalAddressRow``.
    private static let logicalDuplicateCandidatesSQL = """
    SELECT ocId, serverUrl, fileName, normalizedServerUrl, normalizedFileName, deleted, isLockFileOfLocalOrigin, status, syncTime
    FROM itemMetadata
    WHERE normalizedFileName = ? AND normalizedServerUrl = ? AND ocId <> ? AND deleted = 0 AND isLockFileOfLocalOrigin = 0
    """

    private static let softDeleteSQL = "UPDATE itemMetadata SET deleted = 1, syncTime = ? WHERE ocId = ?"

    ///
    /// One-shot startup pass that rewrites drifted normalized location keys and soft-deletes rows
    /// that share a logical address, in a single walk of the table.
    ///
    /// - Returns: How many rows had their keys rewritten and how many duplicates were soft-deleted.
    ///
    @discardableResult
    func repairPersistedLogicalAddresses() -> (repaired: Int, evicted: Int) {
        write("Startup repair: write transaction failed.") { db in
            try repairPersistedLogicalAddresses(in: db)
        } ?? (0, 0)
    }

    private func repairPersistedLogicalAddresses(in db: Database) throws -> (repaired: Int, evicted: Int) {
        let rootContainerOcId = NSFileProviderItemIdentifier.rootContainer.rawValue

        struct LogicalKey: Hashable {
            let serverUrl: String
            let fileName: String
        }

        var drifted: [ItemLogicalAddressRow] = []
        var buckets: [LogicalKey: [ItemLogicalAddressRow]] = [:]

        // Bucketing on the keys computed here rather than on the stored ones is what lets a
        // drifted row be repaired and deduplicated in the same walk.
        let rows = try ItemLogicalAddressRow.fetchCursor(db)
        while let row = try rows.next() {
            let serverUrl = row.serverUrl.precomposedStringWithCanonicalMapping
            let fileName = row.fileName.precomposedStringWithCanonicalMapping

            if row.normalizedServerUrl != serverUrl || row.normalizedFileName != fileName {
                drifted.append(row)
            }

            // The exclusions decide which rows may be soft-deleted, not which rows are repaired.
            guard !row.deleted, !row.isLockFileOfLocalOrigin, row.ocId != rootContainerOcId else {
                continue
            }

            buckets[LogicalKey(serverUrl: serverUrl, fileName: fileName), default: []].append(row)
        }

        let collisions = buckets.values.filter { $0.count > 1 }

        guard !drifted.isEmpty || !collisions.isEmpty else {
            return (0, 0)
        }

        if !drifted.isEmpty {
            logger.error(
                "Repairing \(drifted.count) row(s) whose normalized location keys do not match their raw columns."
            )
        }

        let now = Date()
        var evicted = 0

        for row in drifted {
            try ItemMetadataRecord
                .filter(key: row.ocId)
                .updateAll(
                    db,
                    ItemMetadataRecord.Columns.normalizedServerUrl.set(to: row.serverUrl.precomposedStringWithCanonicalMapping),
                    ItemMetadataRecord.Columns.normalizedFileName.set(to: row.fileName.precomposedStringWithCanonicalMapping)
                )
        }

        for group in collisions {
            let settled = group.filter { $0.status == Status.normal.rawValue }

            guard let winner = settled.max(by: { lhs, rhs in
                if lhs.syncTime != rhs.syncTime {
                    return lhs.syncTime < rhs.syncTime
                }

                return lhs.ocId < rhs.ocId
            }) else {
                logger.info("Startup deduplication: all candidates are in-flight, leaving bucket intact.", [
                    .name: group.first?.fileName,
                    .url: group.first?.serverUrl
                ])

                continue
            }

            logger.info("Startup deduplication: kept canonical row.", [
                .item: winner.ocId,
                .name: winner.fileName,
                .url: winner.serverUrl,
                .syncTime: winner.syncTime
            ])

            for candidate in group where candidate.ocId != winner.ocId {
                if candidate.status != Status.normal.rawValue {
                    logger.error("Startup deduplication: skipped in-flight logical duplicate.", [
                        .item: candidate.ocId,
                        .name: candidate.fileName,
                        .url: candidate.serverUrl,
                        .syncTime: candidate.syncTime
                    ])

                    continue
                }

                try db.cachedStatement(sql: Self.softDeleteSQL).execute(arguments: [now.timeIntervalSinceReferenceDate, candidate.ocId])
                evicted += 1

                logger.info("Startup deduplication: evicted duplicate.", [
                    .item: candidate.ocId,
                    .name: candidate.fileName,
                    .url: candidate.serverUrl,
                    .syncTime: now
                ])
            }
        }

        return (drifted.count, evicted)
    }
}

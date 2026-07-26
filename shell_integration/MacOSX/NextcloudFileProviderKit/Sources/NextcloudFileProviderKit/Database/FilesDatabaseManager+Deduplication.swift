//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import RealmSwift

public extension FilesDatabaseManager {
    ///
    /// Soft-delete any other non-deleted rows at the same logical address as
    /// `incoming` whose `ocId` differs.
    ///
    /// Realm uses `ocId` as the primary key of ``RealmItemMetadata``. An
    /// `update: .all` or `update: .modified` upsert therefore dedupes by
    /// `ocId` only. When the server returns the same logical path with a
    /// fresh `ocId` (restore-from-trash, another client recreating the item
    /// during reconnect, an upload finalizer assigning a server-issued
    /// `ocId`), the upsert inserts a second row beside the first. macOS can
    /// only represent one sibling per logical name and renames the second
    /// with a `" 2"` suffix when this surfaces in Finder.
    ///
    /// Call this from inside an active write transaction. The function
    /// mutates the supplied Realm and assumes the caller will commit.
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
    ///   - database: The Realm holding the open write transaction.
    ///   - now: Timestamp stamped on every evicted row so the change
    ///     surfaces through ``pendingWorkingSetChanges(since:)``.
    ///
    /// - Returns: The `ocId` values of rows that were soft-deleted.
    ///
    @discardableResult
    func evictLogicalDuplicates(of incoming: any ItemMetadata, in database: Realm, now: Date = Date()) -> [String] {
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

        let incomingOcId = incoming.ocId
        let incomingAccount = incoming.account
        let incomingServerUrl = incoming.serverUrl
        let incomingFileName = incoming.fileName

        let candidates = database
            .objects(RealmItemMetadata.self)
            .where {
                $0.account == incomingAccount
                    && $0.serverUrl == incomingServerUrl
                    && $0.fileName == incomingFileName
                    && $0.ocId != incomingOcId
                    && !$0.deleted
                    && !$0.isLockFileOfLocalOrigin
            }

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

            candidate.deleted = true
            candidate.syncTime = now
            evicted.append(candidate.ocId)

            logger.info("Evicted logical duplicate.", [
                .item: candidate.ocId,
                .name: candidate.fileName,
                .url: candidate.serverUrl,
                .syncTime: candidate.syncTime
            ])
        }

        return evicted
    }

    ///
    /// One-shot startup pass that heals pre-existing logical duplicates
    /// already persisted in the database.
    ///
    /// Buckets all non-deleted, non-lock-file rows (except the synthetic
    /// root-container row) by `(account, serverUrl, fileName)`. Within each
    /// bucket containing more than one row, picks a winner among the
    /// settled (non-in-flight) rows by greatest ``ItemMetadata/syncTime``
    /// — with the lexicographically greater `ocId` breaking ties — and
    /// soft-deletes every other settled row. In-flight rows are never
    /// touched (the still-running NSURLSession task references the
    /// specific `ocId`) and an `error`-level log records each skip. If a
    /// bucket is entirely in-flight, the whole bucket is left intact; the
    /// next run-time eviction will heal it once the row settles.
    ///
    /// A write transaction is opened only when at least one bucket has
    /// more than one row, so clean databases pay no transaction cost.
    ///
    func cleanupPreexistingLogicalDuplicates() {
        let database = ncDatabase()
        let rootContainerOcId = NSFileProviderItemIdentifier.rootContainer.rawValue

        let candidates = database
            .objects(RealmItemMetadata.self)
            .where {
                !$0.deleted
                    && !$0.isLockFileOfLocalOrigin
                    && $0.ocId != rootContainerOcId
            }

        struct LogicalKey: Hashable {
            let account: String
            let serverUrl: String
            let fileName: String
        }

        var buckets: [LogicalKey: [RealmItemMetadata]] = [:]

        for candidate in candidates {
            let key = LogicalKey(account: candidate.account, serverUrl: candidate.serverUrl, fileName: candidate.fileName)
            buckets[key, default: []].append(candidate)
        }

        let collisions = buckets.values.filter { $0.count > 1 }

        guard !collisions.isEmpty else {
            return
        }

        let now = Date()

        do {
            try database.write {
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

                        candidate.deleted = true
                        candidate.syncTime = now

                        logger.info("Startup deduplication: evicted duplicate.", [
                            .item: candidate.ocId,
                            .name: candidate.fileName,
                            .url: candidate.serverUrl,
                            .syncTime: candidate.syncTime
                        ])
                    }
                }
            }
        } catch {
            logger.error("Startup deduplication: write transaction failed.", [.error: error])
        }
    }
}

//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import RealmSwift

///
/// Batch persistence helpers for ``FilesDatabaseManager``.
///
/// These are used by code paths that ingest many fresh PROPFIND results at once
/// (e.g. a paginated page of direct children) and want to avoid the per-item
/// Realm write-transaction overhead of the single-item helpers.
///
public extension FilesDatabaseManager {
    ///
    /// Add or replace an array of metadata objects while carrying over local-only state,
    /// using a single Realm write transaction for the whole batch.
    ///
    /// This is the batch counterpart to ``addItemMetadataPreservingLocalState(_:preserveVisitedDirectory:)``.
    /// It preserves the same local-only fields and applies the same logical-address duplicate
    /// eviction, but it loads all existing rows up front and commits everything in one write.
    ///
    /// - Parameters:
    ///   - metadatas: The freshly-built metadata objects to persist.
    ///   - preserveVisitedDirectory: When `false`, do not carry over `visitedDirectory` from the
    ///     existing row. Callers that have just visited the directory should pass `false` and
    ///     pre-set `metadata.visitedDirectory = true`.
    ///
    /// - Returns: The merged metadata objects that were persisted, in the same order as the input.
    ///
    @discardableResult
    func addItemMetadatasPreservingLocalState(
        _ metadatas: [SendableItemMetadata],
        preserveVisitedDirectory: Bool = true
    ) -> [SendableItemMetadata] {
        guard !metadatas.isEmpty else {
            return []
        }

        let signposter = EnumerationSignposter.signposter
        let batchState = signposter.beginInterval(
            "BatchWritePage",
            id: signposter.makeSignpostID(),
            "items=\(metadatas.count)"
        )
        defer { signposter.endInterval("BatchWritePage", batchState) }

        let clock = ContinuousClock()
        let readStart = clock.now

        let database = ncDatabase()

        // 1. Load all existing rows for the incoming ocIds in one query.
        let incomingOcIds = Array(Set(metadatas.map(\.ocId)))
        var existingByOcId: [String: RealmItemMetadata] = [:]
        existingByOcId.reserveCapacity(incomingOcIds.count)
        for existing in database.objects(RealmItemMetadata.self).where({ $0.ocId.in(incomingOcIds) }) {
            if existingByOcId[existing.ocId] == nil {
                existingByOcId[existing.ocId] = existing
            }
        }

        // 2. For incoming rows without an ocId match, load logical-address candidates in one query.
        //    The single-item helper only carries over state when exactly one candidate exists.
        struct LogicalKey: Hashable {
            let account: String
            let serverUrl: String
            let fileName: String
        }

        let logicalKeysNeedingLookup = metadatas
            .filter { existingByOcId[$0.ocId] == nil }
            .map { LogicalKey(account: $0.account, serverUrl: $0.serverUrl, fileName: $0.fileName) }

        var existingByLogicalKey: [LogicalKey: RealmItemMetadata] = [:]
        var logicalCandidates: [LogicalKey: [RealmItemMetadata]] = [:]
        if !logicalKeysNeedingLookup.isEmpty {
            let keysSet = Set(logicalKeysNeedingLookup)
            // Realm does not support IN on composite keys, so we query on the indexed
            // normalizedFileName and then filter the small candidate set in memory.
            let candidateFileNames = Array(Set(logicalKeysNeedingLookup.map(\.fileName.precomposedStringWithCanonicalMapping)))
            let candidates = database
                .objects(RealmItemMetadata.self)
                .where { $0.normalizedFileName.in(candidateFileNames) }

            for candidate in candidates {
                let key = LogicalKey(
                    account: candidate.account,
                    serverUrl: candidate.serverUrl,
                    fileName: candidate.fileName
                )
                guard keysSet.contains(key), !candidate.deleted, !candidate.isLockFileOfLocalOrigin else {
                    continue
                }
                // First occurrence wins for preservation, matching `.first(where:)` in the single-item helper.
                if existingByLogicalKey[key] == nil {
                    existingByLogicalKey[key] = candidate
                }
                // Keep all candidates for eviction.
                logicalCandidates[key, default: []].append(candidate)
            }
        }

        let readElapsed = clock.now - readStart

        // 3. Merge local-only state into the incoming metadatas.
        var inheritedKeepDownloadedByServerUrl: [String: Bool] = [:]
        var mergedMetadatas: [SendableItemMetadata] = []
        mergedMetadatas.reserveCapacity(metadatas.count)

        for var metadata in metadatas {
            if let existing = existingByOcId[metadata.ocId] {
                metadata.downloaded = existing.downloaded
                metadata.keepDownloaded = existing.keepDownloaded

                if preserveVisitedDirectory {
                    metadata.visitedDirectory = existing.visitedDirectory
                }

                metadata.lockToken = existing.lockToken
                if metadata.etag == existing.etag {
                    metadata.fileProviderContentVersion = existing.fileProviderContentVersion
                }
            } else {
                let key = LogicalKey(account: metadata.account, serverUrl: metadata.serverUrl, fileName: metadata.fileName)
                if let existing = existingByLogicalKey[key] {
                    metadata.downloaded = existing.downloaded
                    metadata.keepDownloaded = existing.keepDownloaded

                    if preserveVisitedDirectory {
                        metadata.visitedDirectory = existing.visitedDirectory
                    }

                    metadata.lockToken = existing.lockToken
                    if metadata.etag == existing.etag {
                        metadata.fileProviderContentVersion = existing.fileProviderContentVersion
                    }
                } else {
                    // Genuinely new item: inherit the parent's pin.
                    if let cached = inheritedKeepDownloadedByServerUrl[metadata.serverUrl] {
                        metadata.keepDownloaded = cached
                    } else {
                        let inherited = inheritedKeepDownloaded(for: metadata)
                        inheritedKeepDownloadedByServerUrl[metadata.serverUrl] = inherited
                        metadata.keepDownloaded = inherited
                    }
                }
            }

            mergedMetadatas.append(metadata)
        }

        // 4. Evict logical duplicates using the pre-loaded existing rows.
        let now = Date()
        var evictedOcIds = Set<String>()
        var metadatasToEvict: [SendableItemMetadata] = []

        for metadata in mergedMetadatas {
            guard !metadata.isLockFileOfLocalOrigin, !metadata.deleted else {
                continue
            }

            let incomingKey = LogicalKey(account: metadata.account, serverUrl: metadata.serverUrl, fileName: metadata.fileName)
            guard let candidates = logicalCandidates[incomingKey] else {
                continue
            }

            for candidate in candidates {
                guard candidate.ocId != metadata.ocId, !candidate.deleted, !candidate.isLockFileOfLocalOrigin else {
                    continue
                }
                guard candidate.status == Status.normal.rawValue else {
                    logger.error("Skipping eviction of in-flight logical duplicate.", [
                        .item: candidate.ocId,
                        .name: candidate.fileName,
                        .url: candidate.serverUrl,
                        .syncTime: candidate.syncTime
                    ])
                    continue
                }
                guard evictedOcIds.insert(candidate.ocId).inserted else {
                    continue
                }

                var evicted = SendableItemMetadata(value: candidate)
                evicted.deleted = true
                evicted.syncTime = now
                metadatasToEvict.append(evicted)

                logger.info("Evicted logical duplicate.", [
                    .item: candidate.ocId,
                    .name: candidate.fileName,
                    .url: candidate.serverUrl,
                    .syncTime: candidate.syncTime
                ])
            }
        }

        // 5. Single write transaction for creates, updates and evictions.
        let writeStart = clock.now
        do {
            try database.write {
                if !metadatasToEvict.isEmpty {
                    database.add(metadatasToEvict.map { RealmItemMetadata(value: $0) }, update: .modified)
                }
                database.add(mergedMetadatas.map { RealmItemMetadata(value: $0) }, update: .all)

                for metadata in mergedMetadatas {
                    logger.debug("Added item metadata.", [.item: metadata.ocId, .name: metadata.fileName, .url: metadata.serverUrl])
                }
            }
        } catch {
            logger.error("Failed to batch add item metadatas.", [.error: error])
            return metadatas
        }
        let writeElapsed = clock.now - writeStart

        let totalElapsed = clock.now - readStart
        let writeRate = writeElapsed.fpSeconds > 0 ? Double(metadatas.count) / writeElapsed.fpSeconds : 0
        logger.performance(
            "PERF BatchWritePage items=\(metadatas.count) read_ms=\(readElapsed.fpSeconds * 1000) write_ms=\(writeElapsed.fpSeconds * 1000) total_ms=\(totalElapsed.fpSeconds * 1000) items_per_s=\(writeRate)",
            [.url: metadatas.first?.serverUrl ?? ""]
        )

        return mergedMetadatas
    }
}

// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB

extension FilesDatabaseManager {
    /// Use listed server metadata for live children without undoing local deletions.
    func directoryDownloadMetadatas(_ metadatas: [SendableItemMetadata]) throws -> [SendableItemMetadata] {
        try writer.read { db in
            try metadatas.compactMap { metadata in
                guard let existing = try itemMetadata(ocId: metadata.ocId, in: db) else {
                    throw NSFileProviderError(.cannotSynchronize)
                }
                return existing.deleted ? nil : metadata
            }
        }
    }

    /// Restore listed package members while preserving local transfer state.
    func restorePackageDownloadMetadatas(_ metadatas: [SendableItemMetadata], for operation: DownloadOperation) throws {
        guard !metadatas.isEmpty else { return }
        try Self.downloadOperations.withLock { operations in
            guard operations[storeIdentity]?[operation.ocId]?.operation.identifier == operation.identifier else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            try write(durability: .full) { db in
                try operation.cancellation.checkCancellation()
                guard let directory = try itemMetadata(ocId: operation.ocId, in: db), !directory.deleted else {
                    throw NSFileProviderError(.cannotSynchronize)
                }
                for metadata in metadatas {
                    guard var existing = try itemMetadata(ocId: metadata.ocId, in: db) else {
                        try insertItemMetadata(metadata, in: db)
                        continue
                    }
                    if existing.deleted {
                        guard existing.status == Status.normal.rawValue || existing.status == Status.downloadError.rawValue,
                              operations[storeIdentity]?[metadata.ocId] == nil
                        else {
                            throw NSFileProviderError(.cannotSynchronize)
                        }
                        existing.deleted = false
                        try existing.upsertRow(db)
                    }
                }
            }
        }
    }

    /// Register `operation` as the item's download owner and mark the item as downloading.
    /// For a folder child, first check `listedMetadata` from the parent listing against the current row.
    func beginDownload(_ operation: DownloadOperation, listedMetadata: SendableItemMetadata? = nil) -> SendableItemMetadata? {
        let ocId = operation.ocId
        let (metadata, supersededOperation): (SendableItemMetadata?, DownloadOperation?) = Self.downloadOperations.withLock { operations in
            let metadata: SendableItemMetadata? = write("Could not start download in database.", [.item: ocId]) { db -> SendableItemMetadata? in
                guard var item = try itemMetadata(ocId: ocId, in: db), !item.deleted else { return nil }
                if let listedMetadata {
                    // A sibling's transfer may have given another listing time to update this row.
                    guard item.serverUrl.precomposedStringWithCanonicalMapping == listedMetadata.serverUrl.precomposedStringWithCanonicalMapping else {
                        return nil
                    }
                    // A depth-1 read refreshes normal rows and skips rows with unfinished transfers.
                    if item.status == Status.normal.rawValue,
                       item.etag != listedMetadata.etag || !item.hasSameLocation(as: listedMetadata)
                    {
                        return nil
                    }
                }
                item.status = Status.downloading.rawValue
                item.downloaded = false
                item.sessionError = ""
                try item.upsertRow(db)
                return item.metadata
            } ?? nil
            guard let metadata else { return (nil, nil) }
            let supersededOperation = operations[storeIdentity]?[ocId]?.operation
            operations[storeIdentity, default: [:]][ocId] = (operation, metadata.etag, metadata.remotePath())
            return (metadata, supersededOperation)
        }
        // Cancellation resumes the old fetch, which may need the ownership lock for cleanup.
        if let supersededOperation, supersededOperation.identifier != operation.identifier {
            supersededOperation.supersede()
        }
        return metadata
    }

    /// Check a staged download without publishing or releasing its ownership.
    func validateDownload(_ operation: DownloadOperation) throws {
        try Self.downloadOperations.withLock { operations in
            guard operations[storeIdentity]?[operation.ocId]?.operation.identifier == operation.identifier else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            try writer.read { db in
                guard let item = try itemMetadata(ocId: operation.ocId, in: db), !item.deleted else {
                    throw NSFileProviderError(.cannotSynchronize)
                }
            }
        }
    }

    /// Publish the listed content and the folder's download state in one transaction.
    @discardableResult
    func finishDirectoryDownload(
        _ operation: DownloadOperation,
        children: [DownloadOperation],
        metadatas: [String: SendableItemMetadata]
    ) throws -> SendableItemMetadata {
        try Self.downloadOperations.withLock { operations in
            let downloads = [operation] + children
            guard let activeDownloads = operations[storeIdentity],
                  downloads.allSatisfy({ activeDownloads[$0.ocId]?.operation.identifier == $0.identifier })
            else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            let completed: SendableItemMetadata
            do {
                completed = try write(durability: .full) { db in
                    // Cancellation can arrive while this transaction waits for the writer.
                    try operation.cancellation.checkCancellation()
                    let items = try downloads.map { download in
                        guard let item = try itemMetadata(ocId: download.ocId, in: db), !item.deleted else {
                            if download.identifier == operation.identifier {
                                throw NSError.fileProviderErrorForNonExistentItem(withIdentifier: NSFileProviderItemIdentifier(operation.ocId))
                            }
                            throw NSFileProviderError(.cannotSynchronize)
                        }
                        guard let listed = metadatas[download.ocId],
                              listed.account == item.account,
                              let started = activeDownloads[download.ocId]
                        else {
                            throw NSFileProviderError(.cannotSynchronize)
                        }
                        // A retry can start with a stale row. Use the listing when that row's
                        // version and location have stayed the same during the transfer.
                        let unchangedSinceStart = item.etag == started.etag && item.hasSameRemotePath(as: started.remotePath)
                        let matchesListing = item.etag == listed.etag && item.hasSameLocation(as: listed)
                        guard unchangedSinceStart || matchesListing else {
                            throw NSFileProviderError(.cannotSynchronize)
                        }

                        // Retain current local state and the detected content type.
                        var metadata = item.metadata
                        if metadata.etag != listed.etag {
                            metadata.fileProviderContentVersion = nil
                        }
                        metadata.etag = listed.etag
                        metadata.size = listed.size
                        metadata.date = listed.date
                        metadata.creationDate = listed.creationDate
                        metadata.checksums = listed.checksums
                        metadata.fileName = listed.fileName
                        metadata.fileNameView = listed.fileNameView
                        metadata.name = listed.name
                        metadata.serverUrl = listed.serverUrl
                        metadata.path = listed.path
                        metadata.status = Status.normal.rawValue
                        metadata.downloaded = true
                        metadata.uploaded = true
                        metadata.sessionError = ""
                        return metadata
                    }
                    for metadata in items {
                        // Enumeration can leave an in-flight row at an address now occupied by another item.
                        let addressAvailable = try ItemMetadataRecord
                            .filter(ItemMetadataRecord.hasLocation(serverUrl: metadata.serverUrl, fileName: metadata.fileName))
                            .filter(ItemMetadataRecord.Columns.account == metadata.account && ItemMetadataRecord.Columns.ocId != metadata.ocId)
                            .filter(ItemMetadataRecord.Columns.deleted == false && ItemMetadataRecord.Columns.isLockFileOfLocalOrigin == false)
                            .isEmpty(db)
                        guard addressAvailable else {
                            throw NSFileProviderError(.cannotSynchronize)
                        }
                        // Rebuild the row so its indexed location keys follow the listed path.
                        try ItemMetadataRecord(metadata).upsertRow(db)
                    }
                    try operation.cancellation.checkCancellation()
                    // Register before commit so reconciliation protects these rows until File Provider confirms them.
                    for download in downloads {
                        pendingMaterializations.recordDownloaded(NSFileProviderItemIdentifier(download.ocId))
                    }
                    return items[0]
                }
            } catch {
                logger.error("Could not finish directory download in database.", [.item: operation.ocId, .error: error])
                throw error
            }
            for download in downloads {
                operations[storeIdentity]?.removeValue(forKey: download.ocId)
            }
            if operations[storeIdentity]?.isEmpty == true {
                operations.removeValue(forKey: storeIdentity)
            }
            return completed
        }
    }

    /// Save and return current metadata only while this operation owns the item.
    @discardableResult
    func finishDownload(
        _ operation: DownloadOperation,
        status: Status,
        downloaded: Bool,
        error: String? = nil,
        contentType: String? = nil,
        expectedETag: String? = nil
    ) throws -> SendableItemMetadata {
        let ocId = operation.ocId
        let statusValue = status.rawValue
        return try Self.downloadOperations.withLock { operations in
            guard operations[storeIdentity]?[ocId]?.operation.identifier == operation.identifier else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            // Keep ownership if publishing the download fails so the caller can clean up.
            var releaseOwnership = !downloaded
            defer {
                if releaseOwnership {
                    operations[storeIdentity]?.removeValue(forKey: ocId)
                    if operations[storeIdentity]?.isEmpty == true {
                        operations.removeValue(forKey: storeIdentity)
                    }
                }
            }
            do {
                let metadata = try write(durability: .full) { db in
                    // Cleanup must be able to write even after the fetch is cancelled.
                    if downloaded {
                        try operation.cancellation.checkCancellation()
                    }
                    guard var item = try itemMetadata(ocId: ocId, in: db), !item.deleted else {
                        throw NSError.fileProviderErrorForNonExistentItem(withIdentifier: NSFileProviderItemIdentifier(ocId))
                    }
                    // A folder listing can update the ETag while the download is in flight.
                    if let expectedETag, item.etag != expectedETag {
                        throw NSFileProviderError(.cannotSynchronize)
                    }
                    item.status = statusValue
                    item.downloaded = downloaded
                    item.sessionError = error ?? ""
                    if downloaded {
                        item.uploaded = true
                        if let contentType {
                            item.contentType = contentType
                        }
                    }
                    try item.upsertRow(db)
                    if downloaded {
                        try operation.cancellation.checkCancellation()
                        pendingMaterializations.recordDownloaded(NSFileProviderItemIdentifier(ocId))
                    }
                    return item.metadata
                }
                releaseOwnership = true
                return metadata
            } catch {
                logger.error("Could not finish download in database.", [.item: ocId, .error: error])
                throw error
            }
        }
    }
}

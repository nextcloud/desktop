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
            guard operations[storeIdentity]?[operation.ocId]?.identifier == operation.identifier else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            try write(durability: .full) { db in
                guard !operation.cancellation.isCancelled else { throw CocoaError(.userCancelled) }
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

    func beginDownload(_ operation: DownloadOperation) -> SendableItemMetadata? {
        let ocId = operation.ocId
        let (metadata, supersededOperation): (SendableItemMetadata?, DownloadOperation?) = Self.downloadOperations.withLock { operations in
            let metadata: SendableItemMetadata? = write("Could not start download in database.", [.item: ocId]) { db -> SendableItemMetadata? in
                guard var item = try itemMetadata(ocId: ocId, in: db), !item.deleted else { return nil }
                item.status = Status.downloading.rawValue
                item.downloaded = false
                item.sessionError = ""
                try item.upsertRow(db)
                return item.metadata
            } ?? nil
            guard let metadata else { return (nil, nil) }
            let supersededOperation = operations[storeIdentity]?[ocId]
            operations[storeIdentity, default: [:]][ocId] = operation
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
            guard operations[storeIdentity]?[operation.ocId]?.identifier == operation.identifier else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            try writer.read { db in
                guard let item = try itemMetadata(ocId: operation.ocId, in: db), !item.deleted else {
                    throw NSFileProviderError(.cannotSynchronize)
                }
            }
        }
    }

    /// Publish the folder and its children together only while every download still owns its item.
    func finishDirectoryDownload(_ operation: DownloadOperation, children: [DownloadOperation]) throws {
        try Self.downloadOperations.withLock { operations in
            let downloads = [operation] + children
            guard downloads.allSatisfy({ operations[storeIdentity]?[$0.ocId]?.identifier == $0.identifier }) else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            do {
                try write(durability: .full) { db in
                    let items = try downloads.map { download in
                        guard let item = try itemMetadata(ocId: download.ocId, in: db), !item.deleted else {
                            if download.identifier == operation.identifier {
                                throw NSError.fileProviderErrorForNonExistentItem(withIdentifier: NSFileProviderItemIdentifier(operation.ocId))
                            }
                            throw NSFileProviderError(.cannotSynchronize)
                        }
                        return item
                    }
                    for var item in items {
                        item.status = Status.normal.rawValue
                        item.downloaded = true
                        item.uploaded = true
                        item.sessionError = ""
                        try item.upsertRow(db)
                    }
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
        }
    }

    /// Save the download state only while this operation owns the item.
    func finishDownload(
        _ operation: DownloadOperation,
        status: Status,
        downloaded: Bool,
        error: String? = nil,
        contentType: String? = nil
    ) throws {
        let ocId = operation.ocId
        let statusValue = status.rawValue
        try Self.downloadOperations.withLock { operations in
            guard operations[storeIdentity]?[ocId]?.identifier == operation.identifier else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            defer {
                operations[storeIdentity]?.removeValue(forKey: ocId)
                if operations[storeIdentity]?.isEmpty == true {
                    operations.removeValue(forKey: storeIdentity)
                }
            }
            do {
                try write(durability: .full) { db in
                    guard var item = try itemMetadata(ocId: ocId, in: db), !item.deleted else {
                        throw NSError.fileProviderErrorForNonExistentItem(withIdentifier: NSFileProviderItemIdentifier(ocId))
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
                }
            } catch {
                logger.error("Could not finish download in database.", [.item: ocId, .error: error])
                throw error
            }
        }
    }
}

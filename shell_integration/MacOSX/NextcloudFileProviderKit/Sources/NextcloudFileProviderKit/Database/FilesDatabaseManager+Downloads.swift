// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import RealmSwift

extension FilesDatabaseManager {
    /// Select live children from persisted state without undoing local deletions.
    func directoryDownloadMetadatas(_ metadatas: [SendableItemMetadata]) throws -> [SendableItemMetadata] {
        let database = ncDatabase()
        return try metadatas.compactMap { metadata in
            guard let existing = database.object(ofType: RealmItemMetadata.self, forPrimaryKey: metadata.ocId) else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            return existing.deleted ? nil : SendableItemMetadata(value: existing)
        }
    }

    func beginDownload(_ operation: DownloadOperation) -> SendableItemMetadata? {
        let ocId = operation.ocId
        let (metadata, supersededOperation): (SendableItemMetadata?, DownloadOperation?) = Self.downloadOperations.withLock { operations in
            let database = ncDatabase()
            guard let databaseIdentifier = database.configuration.inMemoryIdentifier ?? database.configuration.fileURL?.absoluteString else { return (nil, nil) }
            var metadata: SendableItemMetadata?
            do {
                try database.write {
                    guard let item = database.object(ofType: RealmItemMetadata.self, forPrimaryKey: ocId), !item.deleted else { return }
                    item.status = Status.downloading.rawValue
                    item.downloaded = false
                    item.sessionError = ""
                    metadata = SendableItemMetadata(value: item)
                }
            } catch {
                logger.error("Could not start download in database.", [.item: ocId, .error: error])
                return (nil, nil)
            }
            guard let metadata else { return (nil, nil) }
            let supersededOperation = operations[databaseIdentifier]?[ocId]
            operations[databaseIdentifier, default: [:]][ocId] = operation
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
            let database = ncDatabase()
            guard let databaseIdentifier = database.configuration.inMemoryIdentifier ?? database.configuration.fileURL?.absoluteString,
                  operations[databaseIdentifier]?[operation.ocId]?.identifier == operation.identifier,
                  let item = database.object(ofType: RealmItemMetadata.self, forPrimaryKey: operation.ocId),
                  !item.deleted
            else {
                throw NSFileProviderError(.cannotSynchronize)
            }
        }
    }

    /// Publish the folder and its children together only while every download still owns its item.
    func finishDirectoryDownload(_ operation: DownloadOperation, children: [DownloadOperation]) throws {
        try Self.downloadOperations.withLock { operations in
            let database = ncDatabase()
            guard let databaseIdentifier = database.configuration.inMemoryIdentifier ?? database.configuration.fileURL?.absoluteString else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            let downloads = [operation] + children
            guard downloads.allSatisfy({ operations[databaseIdentifier]?[$0.ocId]?.identifier == $0.identifier }) else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            do {
                try database.write {
                    let items = try downloads.map { download in
                        guard let item = database.object(ofType: RealmItemMetadata.self, forPrimaryKey: download.ocId), !item.deleted else {
                            if download.identifier == operation.identifier {
                                throw NSError.fileProviderErrorForNonExistentItem(withIdentifier: NSFileProviderItemIdentifier(operation.ocId))
                            }
                            throw NSFileProviderError(.cannotSynchronize)
                        }
                        return item
                    }
                    for item in items {
                        item.status = Status.normal.rawValue
                        item.downloaded = true
                        item.uploaded = true
                        item.sessionError = ""
                    }
                }
            } catch {
                logger.error("Could not finish directory download in database.", [.item: operation.ocId, .error: error])
                throw error
            }
            for download in downloads {
                operations[databaseIdentifier]?.removeValue(forKey: download.ocId)
            }
            if operations[databaseIdentifier]?.isEmpty == true {
                operations.removeValue(forKey: databaseIdentifier)
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
            let database = ncDatabase()
            guard let databaseIdentifier = database.configuration.inMemoryIdentifier ?? database.configuration.fileURL?.absoluteString, operations[databaseIdentifier]?[ocId]?.identifier == operation.identifier else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            defer {
                operations[databaseIdentifier]?.removeValue(forKey: ocId)
                if operations[databaseIdentifier]?.isEmpty == true {
                    operations.removeValue(forKey: databaseIdentifier)
                }
            }
            do {
                try database.write {
                    guard let item = database.object(ofType: RealmItemMetadata.self, forPrimaryKey: ocId), !item.deleted else {
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
                }
            } catch {
                logger.error("Could not finish download in database.", [.item: ocId, .error: error])
                throw error
            }
        }
    }
}

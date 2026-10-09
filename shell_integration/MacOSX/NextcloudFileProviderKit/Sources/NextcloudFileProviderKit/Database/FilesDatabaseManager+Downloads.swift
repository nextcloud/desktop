// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import RealmSwift

extension FilesDatabaseManager {
    func beginDownload(ocId: String, identifier: UUID) -> SendableItemMetadata? {
        Self.downloadOperations.withLock { operations in
            let database = ncDatabase()
            guard let databaseIdentifier = database.configuration.inMemoryIdentifier ?? database.configuration.fileURL?.absoluteString else { return nil }
            var metadata: SendableItemMetadata?
            do {
                try database.write {
                    guard let item = database.object(ofType: RealmItemMetadata.self, forPrimaryKey: ocId), !item.deleted else { return }
                    item.status = Status.downloading.rawValue
                    item.downloaded = false
                    item.sessionError = ""
                    metadata = SendableItemMetadata(value: item)
                }
                if metadata != nil {
                    operations[databaseIdentifier, default: [:]][ocId] = identifier
                }
            } catch {
                logger.error("Could not start download in database.", [.item: ocId, .error: error])
                return nil
            }
            return metadata
        }
    }

    /// Save the download state only while this operation owns the item.
    func finishDownload(
        ocId: String,
        identifier: UUID,
        status: Status,
        downloaded: Bool,
        error: String? = nil,
        contentType: String? = nil
    ) throws {
        let statusValue = status.rawValue
        try Self.downloadOperations.withLock { operations in
            let database = ncDatabase()
            guard let databaseIdentifier = database.configuration.inMemoryIdentifier ?? database.configuration.fileURL?.absoluteString else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            guard operations[databaseIdentifier]?[ocId] == identifier else {
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

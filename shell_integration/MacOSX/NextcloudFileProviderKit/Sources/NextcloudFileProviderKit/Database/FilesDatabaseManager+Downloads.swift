// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

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

    /// Update only the current download's state, preserving metadata changed while it was running.
    func finishDownload(
        ocId: String,
        identifier: UUID,
        status: Status,
        error: String? = nil,
        contentType: String? = nil
    ) {
        let statusValue = status.rawValue
        let downloaded = status == .normal
        Self.downloadOperations.withLock { operations in
            let database = ncDatabase()
            guard let databaseIdentifier = database.configuration.inMemoryIdentifier ?? database.configuration.fileURL?.absoluteString else { return }
            guard operations[databaseIdentifier]?[ocId] == identifier else { return }
            defer {
                operations[databaseIdentifier]?.removeValue(forKey: ocId)
                if operations[databaseIdentifier]?.isEmpty == true {
                    operations.removeValue(forKey: databaseIdentifier)
                }
            }
            do {
                try database.write {
                    guard let item = database.object(ofType: RealmItemMetadata.self, forPrimaryKey: ocId), !item.deleted else { return }
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
            }
        }
    }
}

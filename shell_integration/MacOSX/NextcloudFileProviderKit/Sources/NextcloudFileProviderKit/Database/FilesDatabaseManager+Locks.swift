//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB
import NextcloudKit

///
/// Lock state of items addressed by the raw location an application's lock file names.
///
/// These lookups compare the stored `serverUrl` and `fileName` columns as they are, without Unicode normalization and without excluding deleted items, because lock file handling matches the name an application wrote byte for byte.
///
extension FilesDatabaseManager {
    /// The item stored at exactly this raw location, deleted or not. A live row wins over a deleted one.
    func itemMetadata(rawServerUrl: String, rawFileName: String) -> SendableItemMetadata? {
        read("Could not look up an item by its raw location.", [.url: rawServerUrl, .name: rawFileName]) { db in
            try rawLocationMatch(serverUrl: rawServerUrl, fileName: rawFileName, in: db)?.metadata
        } ?? nil
    }

    /// Store a lock acquired from the server on the item at the raw location.
    ///
    /// Returns `false` when no item is stored there. Throws when the write fails.
    ///
    @discardableResult
    func applyLock(_ lock: NKLock, rawServerUrl: String, rawFileName: String) throws -> Bool {
        try writer.write { db in
            guard var target = try rawLocationMatch(serverUrl: rawServerUrl, fileName: rawFileName, in: db) else {
                return false
            }

            target.lock = true
            target.lockOwner = lock.owner
            target.lockOwnerDisplayName = lock.ownerDisplayName
            target.lockOwnerEditor = lock.ownerEditor
            target.lockOwnerType = lock.ownerType.rawValue
            target.lockTime = lock.time
            target.lockTimeOut = lock.timeOut

            if let etag = lock.etag {
                // LOCK changes server metadata, not file bytes. Keep the content version File Provider already knows while adopting the lock response's etag.
                if target.fileProviderContentVersion == nil {
                    target.fileProviderContentVersion = target.etag
                }

                target.etag = etag
            }

            target.lockToken = lock.token
            // Ensure token-dependent capabilities are published even if the etag is unchanged.
            target.syncTime = Date()

            try target.update(db)
            return true
        }
    }

    /// Remove every lock property from the item at the raw location.
    ///
    /// Returns `false` when no item is stored there. Throws when the write fails.
    ///
    @discardableResult
    func clearLock(rawServerUrl: String, rawFileName: String) throws -> Bool {
        try writer.write { db in
            guard var target = try rawLocationMatch(serverUrl: rawServerUrl, fileName: rawFileName, in: db) else {
                return false
            }

            target.lock = false
            target.lockOwner = nil
            target.lockOwnerDisplayName = nil
            target.lockOwnerEditor = nil
            target.lockOwnerType = nil
            target.lockTime = nil
            target.lockTimeOut = nil
            target.lockToken = nil

            try target.update(db)
            return true
        }
    }

    private func rawLocationMatch(serverUrl: String, fileName: String, in db: Database) throws -> ItemMetadataRecord? {
        try ItemMetadataRecord
            .filter(ItemMetadataRecord.Columns.serverUrl == serverUrl && ItemMetadataRecord.Columns.fileName == fileName)
            .order(ItemMetadataRecord.Columns.deleted, ItemMetadataRecord.Columns.ocId)
            .fetchOne(db)
    }
}

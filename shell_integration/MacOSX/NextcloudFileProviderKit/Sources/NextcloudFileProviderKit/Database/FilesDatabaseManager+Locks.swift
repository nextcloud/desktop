//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudKit
import RealmSwift

///
/// Lock state of items addressed by the raw location an application's lock file names.
///
/// These lookups compare the stored `serverUrl` and `fileName` columns as they are, without Unicode normalization and without excluding deleted items, because lock file handling matches the name an application wrote byte for byte.
///
extension FilesDatabaseManager {
    /// The first item stored at exactly this raw location, deleted or not.
    func itemMetadata(rawServerUrl: String, rawFileName: String) -> SendableItemMetadata? {
        guard let metadata = rawLocationMatches(serverUrl: rawServerUrl, fileName: rawFileName).first else {
            return nil
        }

        return SendableItemMetadata(value: metadata)
    }

    /// Store a lock acquired from the server on the item at the raw location.
    ///
    /// Returns `false` when no item is stored there. Throws when the write fails.
    ///
    @discardableResult
    func applyLock(_ lock: NKLock, rawServerUrl: String, rawFileName: String) throws -> Bool {
        guard let target = rawLocationMatches(serverUrl: rawServerUrl, fileName: rawFileName).first else {
            return false
        }

        try ncDatabase().write {
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
        }

        return true
    }

    /// Remove every lock property from the item at the raw location.
    ///
    /// Returns `false` when no item is stored there. Throws when the write fails.
    ///
    @discardableResult
    func clearLock(rawServerUrl: String, rawFileName: String) throws -> Bool {
        guard let target = rawLocationMatches(serverUrl: rawServerUrl, fileName: rawFileName).first else {
            return false
        }

        try ncDatabase().write {
            target.lock = false
            target.lockOwner = nil
            target.lockOwnerDisplayName = nil
            target.lockOwnerEditor = nil
            target.lockOwnerType = nil
            target.lockTime = nil
            target.lockTimeOut = nil
            target.lockToken = nil
        }

        return true
    }

    private func rawLocationMatches(serverUrl: String, fileName: String) -> Results<RealmItemMetadata> {
        itemMetadatas.where { $0.serverUrl == serverUrl && $0.fileName == fileName }
    }
}

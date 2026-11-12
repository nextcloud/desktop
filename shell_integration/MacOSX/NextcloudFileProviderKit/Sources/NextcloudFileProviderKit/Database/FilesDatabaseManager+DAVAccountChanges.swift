//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import GRDB

extension FilesDatabaseManager {
    /// Check the database identity and look up the item in the same read transaction.
    /// Return nil when the current database has no item with this ID.
    func itemMetadataIfCurrentDAVAccount(
        _ identifier: NSFileProviderItemIdentifier,
        retryError: any Error = NSFileProviderError(.cannotSynchronize)
    ) throws -> SendableItemMetadata? {
        let lookup = read("Could not read item metadata for File Provider.", [.item: identifier.rawValue]) {
            db -> (metadata: SendableItemMetadata?, isCurrent: Bool) in
            guard try StoreIdentityRecord.fetchOne(db)?.identity == storeIdentity else { return (nil, false) }
            return (try itemMetadata(ocId: identifier.rawValue, in: db)?.metadata, true)
        }
        guard let lookup, lookup.isCurrent else { throw retryError }
        return lookup.metadata
    }
}

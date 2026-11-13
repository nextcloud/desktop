//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import GRDB

/// The database identifier changes when the domain switches its DAV server URL or backend user ID.
extension FilesDatabaseManager {
    /// Commit the new account in the open database after the previous account's writes finish.
    /// The write checks the old store identity and rolls back every change together on failure.
    func replacingDAVAccount(with account: Account) throws -> FilesDatabaseManager {
        let storeIdentity = try write(durability: .full) { db in
            if matchesDAVAccount(account) {
                // A credentials-only change keeps the same DAV paths. Cancelled transfers
                // still need their status cleared before the old manager is invalidated.
                try ItemMetadataRecord
                    .filter(ItemMetadataRecord.hasActiveTransfer)
                    .updateAll(db, ItemMetadataRecord.interruptedTransferAssignments)
            }
            return try StoreIdentityRecord.updateDatabaseForCurrentDAVAccount(account, in: db)
        }
        return FilesDatabaseManager(account: account, continuingFrom: self, storeIdentity: storeIdentity)
    }

    private func matchesDAVAccount(_ account: Account) -> Bool {
        self.account.davAccountIdentity.matchesByteForByte(account.davAccountIdentity)
    }

    /// Check that this manager can still use the DAV account it opened the database with.
    /// Another manager can switch the database to a different account and change its identifier.
    func isForCurrentDAVAccount() -> Bool {
        guard !wasInvalidatedByDAVAccountChange else { return false }
        return read("Could not check whether this database manager still uses the current DAV account.") { db in
            try StoreIdentityRecord.fetchOne(db)?.identity == storeIdentity
        } ?? false
    }

    /// Check the database identity and look up the item in the same read transaction.
    /// Return nil when the current database has no item with this ID.
    func itemMetadataIfCurrentDAVAccount(
        _ identifier: NSFileProviderItemIdentifier,
        retryError: any Error = NSFileProviderError(.cannotSynchronize)
    ) throws -> SendableItemMetadata? {
        guard !wasInvalidatedByDAVAccountChange else { throw retryError }
        let lookup = read("Could not read item metadata for File Provider.", [.item: identifier.rawValue]) {
            db -> (metadata: SendableItemMetadata?, isCurrent: Bool) in
            guard try StoreIdentityRecord.fetchOne(db)?.identity == storeIdentity else { return (nil, false) }
            return (try itemMetadata(ocId: identifier.rawValue, in: db)?.metadata, true)
        }
        guard let lookup, lookup.isCurrent else { throw retryError }
        return lookup.metadata
    }
}

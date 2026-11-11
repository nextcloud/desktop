//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import GRDB

extension FilesDatabaseManager {
    /// Live rows that startup could not assign to the current WebDAV account.
    var unresolvedDAVAccountRows: SQLExpression {
        let columns = ItemMetadataRecord.Columns.self
        let sameUserOnPreviousServer = columns.user == account.username
            && columns.userId == account.id
            && columns.urlBase != account.serverUrl
            && columns.urlBase != ""
        let outdatedAbsoluteParent = !ItemMetadataRecord.hasStoredDAVRelativeParent
            && columns.account == account.ncKitAccount
            && !matchesDomainDAVAccountIncludingLegacyURLs

        return !columns.deleted
            && !columns.isLockFileOfLocalOrigin
            && !ItemMetadataRecord.isFilesRoot
            && (sameUserOnPreviousServer || outdatedAbsoluteParent)
    }

    /// Return nil for a missing item. Ask Finder to retry if the row still has outdated
    /// account details or the database has been rebound since this manager opened it.
    func itemMetadataOrRetryForOutdatedAccount(
        _ identifier: NSFileProviderItemIdentifier,
        retryError: any Error = NSFileProviderError(.cannotSynchronize)
    ) throws -> SendableItemMetadata? {
        let ocId = identifier.rawValue
        let lookup = read("Could not read item metadata for File Provider.", [.item: ocId]) {
            db -> (metadata: SendableItemMetadata?, needsRetry: Bool) in
            guard try StoreIdentityRecord.fetchOne(db)?.identity == storeIdentity else { return (nil, true) }
            if let current = try itemMetadata(ocId: ocId, in: db) {
                return (current.metadata, false)
            }
            let unresolved = try !ItemMetadataRecord
                .filter(key: ocId)
                .filter(unresolvedDAVAccountRows)
                .isEmpty(db)
            return (nil, unresolved)
        }
        guard let lookup, !lookup.needsRetry else { throw retryError }
        return lookup.metadata
    }

    /// Read the current working set and whether reporting it alone would omit cached items
    /// with outdated account details.
    func itemMetadataForWorkingSet() -> (
        items: [SendableItemMetadata],
        hasUnresolvedItems: Bool
    )? {
        read("Could not read item metadata for the working set.") { db in
            let hasUnresolvedItems = try !ItemMetadataRecord
                .filter(unresolvedDAVAccountRows && ItemMetadataRecord.isMaterialised)
                .isEmpty(db)
            return (try materialisedItemMetadatas(in: db), hasUnresolvedItems)
        }
    }
}

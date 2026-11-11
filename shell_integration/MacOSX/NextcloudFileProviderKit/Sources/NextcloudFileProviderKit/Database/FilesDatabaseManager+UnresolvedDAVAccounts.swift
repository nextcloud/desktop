//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
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

    /// A listing can confirm one row when startup cannot identify a single previous server URL.
    /// Bind the cached row to the current account only when the listed ID, type and path match.
    /// The listing's usual merge still decides which remote and local fields to keep.
    func rebindUnresolvedItemFromDAVListing(
        _ listed: SendableItemMetadata,
        itemIDsToDelete: Set<String> = [],
        in db: Database
    ) throws -> ItemMetadataRecord? {
        guard listed.uploaded,
              !listed.deleted,
              !listed.isFilesRoot,
              listed.account.utf8.elementsEqual(account.ncKitAccount.utf8),
              listed.urlBase.utf8.elementsEqual(account.serverUrl.utf8),
              listed.user.utf8.elementsEqual(account.username.utf8),
              listed.userId.utf8.elementsEqual(account.id.utf8),
              let listedParent = account.davRelativePath(forURL: listed.serverUrl),
              !Account.isStoredTrashLocation(listedParent),
              !listedParent.split(separator: "/").contains(".."),
              !listed.fileName.isEmpty,
              listed.fileName != ".",
              listed.fileName != "..",
              !listed.fileName.contains("/")
        else { return nil }

        guard var cached = try ItemMetadataRecord
            .filter(key: listed.ocId)
            .filter(unresolvedDAVAccountRows)
            .fetchRecord(db, logger: logger),
              let cachedParent = cached.storedParentPath,
              cachedParent.utf8.elementsEqual(listedParent.utf8),
              cached.fileName.utf8.elementsEqual(listed.fileName.utf8),
              cached.directory == listed.directory,
              cached.user.utf8.elementsEqual(account.username.utf8),
              cached.userId.utf8.elementsEqual(account.id.utf8),
              URL(string: cached.urlBase)?.host != nil,
              !cached.urlBase.utf8.elementsEqual(account.serverUrl.utf8),
              cached.account.utf8.elementsEqual(
                  Account.ncKitAccountString(from: cached.user, serverUrl: cached.urlBase).utf8
              )
        else { return nil }

        // A complete listing can replace an occupant it will mark deleted in this transaction.
        // An occupant with an unfinished upload stays live and still blocks recovery at this path.
        let occupantIDs = try ItemMetadataRecord
            .filter(matchesParentURLAndFileName(parentURL: listed.serverUrl, fileName: listed.fileName))
            .filter(!ItemMetadataRecord.Columns.deleted && !ItemMetadataRecord.Columns.isLockFileOfLocalOrigin)
            .select(ItemMetadataRecord.Columns.ocId, as: String.self)
            .fetchAll(db)
        guard occupantIDs.allSatisfy(itemIDsToDelete.contains) else { return nil }

        cached.updateLocation(parentURL: listed.serverUrl, fileName: cached.fileName, domainAccount: account)
        cached.lockToken = nil
        try cached.upsertRow(db)
        return cached
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

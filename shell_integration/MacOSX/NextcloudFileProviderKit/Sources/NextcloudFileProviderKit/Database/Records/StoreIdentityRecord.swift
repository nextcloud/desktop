//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

/// The row that identifies this database. The identifier changes when the database switches DAV accounts.
struct StoreIdentityRecord: Codable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "storeIdentity"

    var id = 1
    var identity: String
    /// The JSON-encoded login and DAV root details saved at the last successful account setup.
    var accountBinding: Data?

    /// Record the domain's DAV account and invalidate results derived from the previous DAV root.
    /// Item paths already omit that root, so the new manager can use them without rewriting them.
    static func updateDatabaseForCurrentDAVAccount(
        _ account: Account,
        importedItemsFromPreviousDAVAccount: Bool = false,
        in db: Database
    ) throws -> String {
        var store = try fetchOne(db) ?? StoreIdentityRecord(identity: UUID().uuidString)
        let accountFields = try JSONEncoder().encode(account.davAccountIdentity.serializedFields)
        guard store.accountBinding != accountFields else { return store.identity }

        let changedDAVAccount = store.accountBinding != nil
        if changedDAVAccount {
            // Old lock tokens and active transfer attempts belong to the previous DAV account.
            let lockToken = ItemMetadataRecord.Columns.lockToken
            try ItemMetadataRecord
                .filter(ItemMetadataRecord.hasStoredDAVRelativeParent)
                .filter(lockToken != nil || ItemMetadataRecord.hasActiveTransfer)
                .updateAll(db, [lockToken.set(to: nil)] + ItemMetadataRecord.interruptedTransferAssignments)
        }

        if try changedDAVAccount
            || importedItemsFromPreviousDAVAccount
            || hasQueuedChangesForDifferentAccount(than: account, in: db)
        {
            // Old changes may contain deletions inferred using the previous DAV base.
            _ = try ChangeDeliveryItemRecord.deleteAll(db)
            _ = try ChangeDeliverySessionRecord.deleteAll(db)
            store.identity = UUID().uuidString
        }

        store.accountBinding = accountFields
        try store.upsert(db)
        return store.identity
    }

    private static func hasQueuedChangesForDifferentAccount(than account: Account, in db: Database) throws -> Bool {
        let snapshots = try Row.fetchCursor(
            db,
            sql: """
                SELECT metadataData, deleted
                FROM changeDeliveryItem
                JOIN changeDeliverySession USING (sessionId)
                WHERE changeDeliverySession.completed = 0
                """
        )
        let decoder = JSONDecoder()
        while let snapshot = try snapshots.next() {
            guard let data: Data = try? snapshot.decode(forColumn: "metadataData"),
                  let deleted: Bool = try? snapshot.decode(forColumn: "deleted"),
                  let metadata = try? ChangeDeliveryItemRecord.decodeMetadata(data, using: decoder)
            else { continue }

            if deleted && !account.ownsDAVReadURL(metadata.serverUrl, sourceMetadata: metadata) {
                return true
            }
            if !metadata.account.utf8.elementsEqual(account.ncKitAccount.utf8)
                || !metadata.urlBase.utf8.elementsEqual(account.serverUrl.utf8)
                || !metadata.userId.utf8.elementsEqual(account.id.utf8)
            {
                return true
            }
        }
        return false
    }
}

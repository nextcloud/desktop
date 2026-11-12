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

    /// Update matching cached items to use the current login, server URL, and backend user ID.
    /// Keep their item IDs and paths below the DAV root. The new root is assumed to point to the same
    /// sync location and history. Regular enumeration handles changes to the items themselves.
    /// Change the database identifier so older managers cannot write after the account changes.
    /// Discard queued changes from the previous account because they may include stale deletions.
    static func updateDatabaseForCurrentDAVAccount(
        _ account: Account,
        previouslyConfiguredAccount: DAVAccountIdentity?,
        in db: Database
    ) throws -> String {
        var store = try fetchOne(db) ?? StoreIdentityRecord(identity: UUID().uuidString)
        let currentAccount = account.davAccountIdentity
        let currentDAVAccountFields = try JSONEncoder().encode(currentAccount.serializedFields)
        let savedDAVAccountFields = store.accountBinding
        if savedDAVAccountFields == currentDAVAccountFields {
            return store.identity
        }

        try DatabaseSchema.fillMissingDAVAccountFields(from: currentAccount, in: db)

        let didUpdateItemAccountFields: Bool
        if let savedDAVAccountFields {
            let fields = try? JSONDecoder().decode([String].self, from: savedDAVAccountFields)
            let sourceAccount = fields.flatMap(DAVAccountIdentity.init(serializedFields:)) ?? currentAccount
            if !sourceAccount.matchesByteForByte(currentAccount) {
                try DatabaseSchema.fillMissingDAVAccountFields(from: sourceAccount, in: db)
            }
            let columns = ItemMetadataRecord.Columns.self
            let matchesSavedAccount = columns.account == sourceAccount.accountKey
                && columns.urlBase == sourceAccount.serverBaseURL
                && columns.userId == sourceAccount.backendUserID
            didUpdateItemAccountFields = try updateCachedItems(
                matching: matchesSavedAccount,
                to: currentAccount,
                in: db
            )
        } else {
            didUpdateItemAccountFields = try updateUnboundItems(
                to: currentAccount,
                previouslyConfiguredAccount: previouslyConfiguredAccount,
                in: db
            )
        }
        let shouldDiscardQueuedChanges = try savedDAVAccountFields != nil
            || didUpdateItemAccountFields
            || hasQueuedChangesForDifferentAccount(than: account, in: db)
        if shouldDiscardQueuedChanges {
            // Old changes may contain deletions inferred using the previous DAV base.
            _ = try ChangeDeliveryItemRecord.deleteAll(db)
            _ = try ChangeDeliverySessionRecord.deleteAll(db)
            store.identity = UUID().uuidString
        }

        store.accountBinding = currentDAVAccountFields
        try store.upsert(db)
        return store.identity
    }

    /// Realm staging has no account binding. Each row can identify its own previous DAV root
    /// because the importer stores a relative parent only when it matches that recorded root.
    private static func updateUnboundItems(
        to currentAccount: DAVAccountIdentity,
        previouslyConfiguredAccount: DAVAccountIdentity?,
        in db: Database
    ) throws -> Bool {
        let columns = ItemMetadataRecord.Columns.self
        var matchesDomainLogin = columns.user == currentAccount.username
        if let previous = previouslyConfiguredAccount, !previous.matchesByteForByte(currentAccount) {
            try DatabaseSchema.fillMissingDAVAccountFields(from: previous, in: db)
            // A known account change can also change the login.
            matchesDomainLogin = matchesDomainLogin || (
                columns.account == previous.accountKey
                    && columns.urlBase == previous.serverBaseURL
                    && columns.user == previous.username
                    && columns.userId == previous.backendUserID
            )
        }
        let safeParentPath = SQL("""
            \(columns.serverUrl) NOT GLOB '*/../*'
                AND \(columns.serverUrl) NOT GLOB '*/..'
                AND \(columns.serverUrl) NOT GLOB '*/./*'
                AND \(columns.serverUrl) NOT GLOB '*/.'
            """).sqlExpression
        let safeFileName = !["", ".", ".."].contains(columns.fileName)
            && SQL("instr(\(columns.fileName), '/') = 0").sqlExpression
        let filesRoot = columns.fileName == SendableItemMetadata.filesRootPlaceholderName
            && columns.directory
            && ["files:", "files:/"].contains(columns.serverUrl)
        let eligibleLocation = safeParentPath && (safeFileName || filesRoot)
        let roots = try Row.fetchAll(
            db,
            ItemMetadataRecord
                .filter(ItemMetadataRecord.hasStoredDAVRelativeParent)
                .filter(matchesDomainLogin && eligibleLocation)
                .filter(
                    columns.account != currentAccount.accountKey
                        || columns.urlBase != currentAccount.serverBaseURL
                        || columns.user != currentAccount.username
                        || columns.userId != currentAccount.backendUserID
                )
                .select(columns.account, columns.urlBase, columns.user, columns.userId)
                .distinct()
        )
        var didUpdateItems = false
        for root in roots {
            guard let accountKey: String = try? root.decode(forColumn: columns.account),
                  let serverBaseURL: String = try? root.decode(forColumn: columns.urlBase),
                  let username: String = try? root.decode(forColumn: columns.user),
                  let backendUserID: String = try? root.decode(forColumn: columns.userId),
                  let server = URLComponents(string: serverBaseURL),
                  let scheme = server.scheme?.lowercased(),
                  ["http", "https"].contains(scheme),
                  server.host?.isEmpty == false,
                  server.query == nil,
                  server.fragment == nil,
                  !username.isEmpty,
                  !["", ".", ".."].contains(backendUserID),
                  !backendUserID.contains("/"),
                  accountKey.isEmpty || accountKey.utf8.elementsEqual(
                      Account.ncKitAccountString(from: username, serverUrl: serverBaseURL).utf8
                  )
            else { continue }

            let matchesRecordedRoot = columns.account == accountKey
                && columns.urlBase == serverBaseURL
                && columns.user == username
                && columns.userId == backendUserID
            if try updateCachedItems(
                matching: matchesRecordedRoot && eligibleLocation,
                to: currentAccount,
                in: db
            ) {
                didUpdateItems = true
            }
        }
        return didUpdateItems
    }

    private static func updateCachedItems(
        matching source: SQLExpression,
        to account: DAVAccountIdentity,
        in db: Database
    ) throws -> Bool {
        let columns = ItemMetadataRecord.Columns.self
        let changedDAVRoot = columns.account != account.accountKey
            || columns.urlBase != account.serverBaseURL
            || columns.userId != account.backendUserID
        let lockToken = SQL("CASE WHEN \(changedDAVRoot) THEN NULL ELSE \(columns.lockToken) END").sqlExpression
        let accountAssignments = [
            columns.account.set(to: account.accountKey),
            columns.urlBase.set(to: account.serverBaseURL),
            columns.userId.set(to: account.backendUserID),
            columns.user.set(to: account.username),
            columns.lockToken.set(to: lockToken)
        ]
        let updated = try ItemMetadataRecord
            .filter(ItemMetadataRecord.hasStoredDAVRelativeParent)
            .filter(source)
            .filter(changedDAVRoot || columns.user != account.username)
            .updateAll(db, accountAssignments + ItemMetadataRecord.interruptedTransferAssignments)
        return updated > 0
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

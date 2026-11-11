//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

extension DatabaseSchema {
    /// Fill missing account fields on imported rows whose parents are under this account's
    /// WebDAV roots. Store each matched parent as a `files:` or `trash:` path.
    static func fillMissingDAVAccountFields(
        from account: DAVAccountIdentity,
        in db: Database
    ) throws {
        let columns = ItemMetadataRecord.Columns.self
        let candidates = ItemMetadataRecord.select([
            Column.rowID,
            columns.serverUrl,
            columns.fileName,
            columns.urlBase,
            columns.userId
        ])
        .filter(
            columns.account == ""
                || columns.user == ""
                || columns.urlBase == ""
                || columns.userId == ""
        )
        .filter(columns.account == "" || columns.account == account.accountKey)
        .filter(columns.user == "" || columns.user == account.username)
        .filter(columns.urlBase == "" || columns.urlBase == account.serverBaseURL)
        .filter(columns.userId == "" || columns.userId == account.backendUserID)
        let batchSize = FilesDatabaseManager.inClauseChunkSize
        let updateItemParent = try db.cachedStatement(sql: """
            UPDATE itemMetadata
            SET account = :account,
                serverUrl = :storedParentLocation,
                normalizedServerUrl = :normalizedParentLocation,
                urlBase = :accountBaseURL,
                user = :username,
                userId = :backendUserID,
                fileName = coalesce(:rootName, fileName),
                fileNameView = coalesce(:rootName, fileNameView),
                normalizedFileName = coalesce(:rootName, normalizedFileName),
                directory = directory OR (:rootName IS NOT NULL)
            WHERE rowid = :rowID
            """)

        // Read one batch before writing it. SQLite's integer row ID remains usable even if an ocId is damaged.
        let batch = candidates.order(Column.rowID).limit(batchSize)
        var rows = try Row.fetchAll(db, batch)
        while let last = rows.last {
            for row in rows {
                do {
                    try fillMissingDAVAccountFields(
                        from: account,
                        on: row,
                        using: updateItemParent
                    )
                } catch is RowDecodingError {
                    // Leave unreadable rows for a later server refresh.
                    continue
                }
            }
            let lastRowID: Int64 = last["rowid"]
            rows = try Row.fetchAll(db, batch.filter(Column.rowID > lastRowID))
        }
    }

    private static func fillMissingDAVAccountFields(
        from account: DAVAccountIdentity,
        on row: Row,
        using updateItemParent: Statement
    ) throws {
        let rowID: Int64 = row["rowid"]
        let parentURL: String = try row.decode(forColumn: "serverUrl")
        let fileName: String = try row.decode(forColumn: "fileName")
        let parentIsRelative = Account.isStoredDAVRelativeLocation(parentURL)
        if parentIsRelative {
            // A relative path no longer contains the server or backend ID. Use the row's values to verify its DAV root.
            let serverBaseURL: String = try row.decode(forColumn: "urlBase")
            let backendUserID: String = try row.decode(forColumn: "userId")
            guard serverBaseURL.utf8.elementsEqual(account.serverBaseURL.utf8),
                  backendUserID.utf8.elementsEqual(account.backendUserID.utf8)
            else { return }
        }

        // Older rows may store the installation path, such as /nextcloud, as it appeared in the DAV response.
        let isRoot = Account.isFilesRootAddress(
            parentURL: parentURL,
            fileName: fileName,
            accountBaseURL: account.serverBaseURL,
            backendUserID: account.backendUserID,
            acceptEquivalentResponseWebroot: true
        )
        let relativeParentPath = if isRoot {
            Account.storedFilesRootLocation
        } else if parentIsRelative {
            parentURL
        } else {
            Account.davRelativePath(
                forURL: parentURL,
                accountBaseURL: account.serverBaseURL,
                backendUserID: account.backendUserID,
                acceptEquivalentResponseWebroot: true
            )
        }
        guard let storedParentLocation = relativeParentPath,
              !storedParentLocation.split(separator: "/").contains("..")
        else { return }
        try updateItemParent.execute(arguments: [
            "account": account.accountKey,
            "storedParentLocation": storedParentLocation,
            "normalizedParentLocation": storedParentLocation.precomposedStringWithCanonicalMapping,
            "accountBaseURL": account.serverBaseURL,
            "username": account.username,
            "backendUserID": account.backendUserID,
            "rowID": rowID,
            "rootName": isRoot ? SendableItemMetadata.filesRootPlaceholderName : nil
        ])
    }
}

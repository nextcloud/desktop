//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import GRDB

extension FilesDatabaseManager {
    func trashedItemMetadatas() -> [SendableItemMetadata] {
        read("Could not fetch the trashed item metadata.", [.account: account.ncKitAccount]) { db in
            try ItemMetadataRecord
                .filter(ItemMetadataRecord.Columns.davRoot == DAVRoot.trash.rawValue)
                .fetchRecords(db, account: account, logger: logger)
                .map(\.metadata)
        } ?? []
    }
}

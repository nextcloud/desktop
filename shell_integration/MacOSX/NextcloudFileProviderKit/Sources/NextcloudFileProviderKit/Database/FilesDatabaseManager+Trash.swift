//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import GRDB

extension FilesDatabaseManager {
    func trashedItemMetadatas(account: Account) -> [SendableItemMetadata] {
        read("Could not fetch the trashed item metadata.", [.account: account.ncKitAccount]) { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.Columns.account == account.ncKitAccount
                        && ItemMetadataRecord.hasServerUrl(equalTo: account.trashUrl, includingDescendants: true)
                )
                .fetchAll(db)
                .map(\.metadata)
        } ?? []
    }
}

//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import RealmSwift

extension FilesDatabaseManager {
    func trashedItemMetadatas(account: Account) -> [SendableItemMetadata] {
        ncDatabase()
            .objects(RealmItemMetadata.self)
            .where { item in
                item.account == account.ncKitAccount &&
                    RealmItemMetadata.hasServerUrl(item, equalTo: account.trashUrl, includingDescendants: true)
            }
            .toUnmanagedResults()
    }
}

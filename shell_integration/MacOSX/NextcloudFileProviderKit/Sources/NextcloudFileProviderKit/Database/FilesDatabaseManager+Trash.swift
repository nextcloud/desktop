//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import RealmSwift

extension FilesDatabaseManager {
    func trashedItemMetadatas(account: Account) -> [SendableItemMetadata] {
        ncDatabase()
            .objects(RealmItemMetadata.self)
            .where {
                $0.account == account.ncKitAccount && $0.serverUrl.starts(with: account.trashUrl)
            }
            .toUnmanagedResults()
    }
}

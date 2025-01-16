//
//  FilesDatabaseManager+Trash.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-02.
//

import RealmSwift

extension FilesDatabaseManager {
    func trashedItemMetadatas(account: Account) -> [ItemMetadata] {
        ncDatabase()
            .objects(ItemMetadata.self)
            .where {
                $0.account == account.ncKitAccount && $0.serverUrl.starts(with: account.trashUrl)
            }
            .map { ItemMetadata(value: $0) }
    }
}

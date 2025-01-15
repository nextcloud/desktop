//
//  FilesDatabaseManager+Trash.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-02.
//

extension FilesDatabaseManager {
    func trashedItemMetadatas(account: Account) -> [ItemMetadata] {
        ncDatabase()
            .objects(ItemMetadata.self)
            .filter {
                $0.account == account.ncKitAccount && $0.serverUrl.hasPrefix(account.trashUrl)
            }
            .map { ItemMetadata(value: $0) }
    }
}

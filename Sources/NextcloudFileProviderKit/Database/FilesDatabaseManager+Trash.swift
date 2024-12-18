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
            .filter(
                "account == %@ AND serverUrl BEGINSWITH %@",
                account.ncKitAccount,
                account.trashUrl
            )
            .toUnmanagedResults()
    }
}

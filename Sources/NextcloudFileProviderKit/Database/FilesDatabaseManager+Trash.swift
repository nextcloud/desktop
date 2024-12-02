//
//  FilesDatabaseManager+Trash.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-02.
//

extension FilesDatabaseManager {
    func trashedItemMetadatas(account: String) -> [ItemMetadata] {
        let metadatas = ncDatabase()
            .objects(ItemMetadata.self)
            .filter("account == %@ AND trashbinFileName != ''", account)
        return sortedItemMetadatas(metadatas)
    }
}

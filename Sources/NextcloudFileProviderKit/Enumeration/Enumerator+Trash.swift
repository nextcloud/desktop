//
//  Enumerator+Trash.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-02.
//

import FileProvider
import NextcloudKit

extension Enumerator {
    static func completeEnumerationObserver(
        _ observer: NSFileProviderEnumerationObserver,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        numPage: Int,
        trashItems: [NKTrash]
    ) {
        var metadatas = [ItemMetadata]()
        for trashItem in trashItems {
            let metadata = trashItem.toItemMetadata(account: remoteInterface.account)
            dbManager.addItemMetadata(metadata)
            metadatas.append(metadata)
        }

        Self.metadatasToFileProviderItems(
            metadatas, remoteInterface: remoteInterface, dbManager: dbManager
        ) { items in
            observer.didEnumerate(items)
            Self.logger.info("Did enumerate \(items.count) trash items")
            observer.finishEnumerating(upTo: fileProviderPageforNumPage(numPage))
        }
    }
}

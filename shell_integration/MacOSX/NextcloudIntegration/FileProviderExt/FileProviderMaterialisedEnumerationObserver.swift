/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import Foundation
import FileProvider

class FileProviderMaterialisedEnumerationObserver : NSObject, NSFileProviderEnumerationObserver {
    let ncKitAccount: String
    var allEnumeratedItemIds: Set<String> = Set<String>()

    required init(ncKitAccount: String) {
        self.ncKitAccount = ncKitAccount
        super.init()
    }

    func didEnumerate(_ updatedItems: [NSFileProviderItemProtocol]) {
        let updatedItemsIds = Array(updatedItems.map { $0.itemIdentifier.rawValue })

        for updatedItemsId in updatedItemsIds {
            allEnumeratedItemIds.insert(updatedItemsId)
        }
    }

    func finishEnumerating(upTo nextPage: NSFileProviderPage?) {
        NSLog("Handling enumerated materialised items.")
        FileProviderMaterialisedEnumerationObserver.handleEnumeratedItems(self.allEnumeratedItemIds,
                                                                          account: self.ncKitAccount)
    }

    func finishEnumeratingWithError(_ error: Error) {
        NSLog("Ran into error when enumerating materialised items. Handling items enumerated so far")
        FileProviderMaterialisedEnumerationObserver.handleEnumeratedItems(self.allEnumeratedItemIds,
                                                                          account: self.ncKitAccount)
    }

    static func handleEnumeratedItems(_ itemIds: Set<String>, account: String) {
        let dbManager = NextcloudFilesDatabaseManager.shared
        let databaseLocalFileMetadatas = dbManager.localFileMetadatas(account: account)
        var noLongerMaterialisedIds = Set<String>()

        DispatchQueue.global(qos: .background).async {
            for localFile in databaseLocalFileMetadatas {
                let localFileOcId = localFile.ocId

                guard itemIds.contains(localFileOcId) else {
                    noLongerMaterialisedIds.insert(localFileOcId)
                    continue;
                }
            }

            DispatchQueue.main.async {
                NSLog("Cleaning up local file metadatas for unmaterialised items")
                for itemId in noLongerMaterialisedIds {
                    dbManager.deleteLocalFileMetadata(ocId: itemId)
                }
            }
        }
    }
}

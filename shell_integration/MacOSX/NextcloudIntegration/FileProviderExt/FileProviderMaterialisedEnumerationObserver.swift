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

import FileProvider
import Foundation
import OSLog

class FileProviderMaterialisedEnumerationObserver: NSObject, NSFileProviderEnumerationObserver {
    let ncKitAccount: String
    let completionHandler: (_ deletedOcIds: Set<String>) -> Void
    var allEnumeratedItemIds: Set<String> = .init()

    required init(
        ncKitAccount: String, completionHandler: @escaping (_ deletedOcIds: Set<String>) -> Void
    ) {
        self.ncKitAccount = ncKitAccount
        self.completionHandler = completionHandler
        super.init()
    }

    func didEnumerate(_ updatedItems: [NSFileProviderItemProtocol]) {
        let updatedItemsIds = Array(updatedItems.map(\.itemIdentifier.rawValue))

        for updatedItemsId in updatedItemsIds {
            allEnumeratedItemIds.insert(updatedItemsId)
        }
    }

    func finishEnumerating(upTo _: NSFileProviderPage?) {
        Logger.materialisedFileHandling.debug("Handling enumerated materialised items.")
        FileProviderMaterialisedEnumerationObserver.handleEnumeratedItems(
            allEnumeratedItemIds,
            account: ncKitAccount,
            completionHandler: completionHandler)
    }

    func finishEnumeratingWithError(_ error: Error) {
        Logger.materialisedFileHandling.error(
            "Ran into error when enumerating materialised items: \(error.localizedDescription, privacy: .public). Handling items enumerated so far"
        )
        FileProviderMaterialisedEnumerationObserver.handleEnumeratedItems(
            allEnumeratedItemIds,
            account: ncKitAccount,
            completionHandler: completionHandler)
    }

    static func handleEnumeratedItems(
        _ itemIds: Set<String>, account: String,
        completionHandler: @escaping (_ deletedOcIds: Set<String>) -> Void
    ) {
        let dbManager = NextcloudFilesDatabaseManager.shared
        let databaseLocalFileMetadatas = dbManager.localFileMetadatas(account: account)
        var noLongerMaterialisedIds = Set<String>()

        DispatchQueue.global(qos: .background).async {
            for localFile in databaseLocalFileMetadatas {
                let localFileOcId = localFile.ocId

                guard itemIds.contains(localFileOcId) else {
                    noLongerMaterialisedIds.insert(localFileOcId)
                    continue
                }
            }

            DispatchQueue.main.async {
                Logger.materialisedFileHandling.info(
                    "Cleaning up local file metadatas for unmaterialised items")
                for itemId in noLongerMaterialisedIds {
                    dbManager.deleteLocalFileMetadata(ocId: itemId)
                }

                completionHandler(noLongerMaterialisedIds)
            }
        }
    }
}

/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import FileProvider
import Foundation
import NextcloudFileProviderKit
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
        let dbManager = FilesDatabaseManager.shared
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

//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import RealmSwift

public class MaterialisedEnumerationObserver: NSObject, NSFileProviderEnumerationObserver {
    let logger: FileProviderLogger
    public let ncKitAccount: String
    let dbManager: FilesDatabaseManager
    private let completionHandler: (
        _ materialisedIds: Set<String>, _ unmaterialisedIds: Set<String>
    ) -> Void
    private var allEnumeratedItemIds = Set<String>()

    public required init(
        ncKitAccount: String,
        dbManager: FilesDatabaseManager,
        log: any FileProviderLogging,
        completionHandler: @escaping (
            _ materialisedIds: Set<String>, _ unmaterialisedIds: Set<String>
        ) -> Void
    ) {
        self.ncKitAccount = ncKitAccount
        self.dbManager = dbManager
        self.logger = FileProviderLogger(category: "MaterialisedEnumerationObserver", log: log)
        self.completionHandler = completionHandler
        super.init()
    }

    public func didEnumerate(_ updatedItems: [NSFileProviderItemProtocol]) {
        updatedItems.map(\.itemIdentifier.rawValue).forEach { allEnumeratedItemIds.insert($0) }
    }

    public func finishEnumerating(upTo _: NSFileProviderPage?) {
        logger.debug("Handling enumerated materialised items.")

        handleEnumeratedItems(
            allEnumeratedItemIds,
            account: ncKitAccount,
            dbManager: dbManager,
            completionHandler: completionHandler
        )
    }

    public func finishEnumeratingWithError(_ error: Error) {
        logger.error("Finishing enumeration with error.", [.error: error])

        handleEnumeratedItems(
            allEnumeratedItemIds,
            account: ncKitAccount,
            dbManager: dbManager,
            completionHandler: completionHandler
        )
    }

    func handleEnumeratedItems(
        _ itemIds: Set<String>,
        account: String,
        dbManager: FilesDatabaseManager,
        completionHandler: @escaping (
            _ materialisedIds: Set<String>, _ unmaterialisedIds: Set<String>
        ) -> Void
    ) {
        let materialisedMetadatas = dbManager.materialisedItemMetadatas(account: account)
        var materialisedMetadatasMap = [String: SendableItemMetadata]()
        var unmaterialisedIds = Set<String>()
        var newMaterialisedIds = Set<String>()

        materialisedMetadatas.forEach {
            materialisedMetadatasMap[$0.ocId] = $0
            unmaterialisedIds.insert($0.ocId)
        }

        for enumeratedId in itemIds {
            if unmaterialisedIds.contains(enumeratedId) {
                unmaterialisedIds.remove(enumeratedId)
            } else {
                newMaterialisedIds.insert(enumeratedId)

                guard var metadata = dbManager.itemMetadata(ocId: enumeratedId) else {
                    logger.error("No metadata for \(enumeratedId) found", [.ocId: enumeratedId])
                    continue
                }

                if metadata.directory {
                    metadata.visitedDirectory = true
                } else {
                    metadata.downloaded = true
                }

                logger.info("Updating materialisation state for item to MATERIALISED with id \(enumeratedId) with filename \(metadata.fileName)", [.ocId: enumeratedId, .name: metadata.fileName])
                dbManager.addItemMetadata(metadata)
            }
        }

        for unmaterialisedId in unmaterialisedIds {
            guard var metadata = materialisedMetadatasMap[unmaterialisedId] else {
                logger.error("No materialised for \(unmaterialisedId) found.", [.ocId: unmaterialisedId])
                continue
            }

            logger.info("Updating materialisation state for item to DATALESS with id \(unmaterialisedId) with filename \(metadata.fileName).", [.name: metadata.fileName, .ocId: unmaterialisedId])

            metadata.downloaded = false
            metadata.visitedDirectory = false
            dbManager.addItemMetadata(metadata)
        }

        // TODO: Do we need to signal the working set now? Unclear

        completionHandler(newMaterialisedIds, unmaterialisedIds)
    }
}

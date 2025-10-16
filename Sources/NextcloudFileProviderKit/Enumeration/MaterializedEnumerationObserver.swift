//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
import RealmSwift

///
/// The custom `NSFileProviderEnumerationObserver` implementation to process materialized items enumerated by the system.
///
public class MaterializedEnumerationObserver: NSObject, NSFileProviderEnumerationObserver {
    let logger: FileProviderLogger
    public let account: Account
    let dbManager: FilesDatabaseManager
    private let completionHandler: (_ materialized: Set<NSFileProviderItemIdentifier>, _ evicted: Set<NSFileProviderItemIdentifier>) -> Void

    ///
    /// All materialized items enumerated by the system.
    ///
    private var enumeratedItems = Set<NSFileProviderItemIdentifier>()

    public required init(account: Account, dbManager: FilesDatabaseManager, log: any FileProviderLogging, completionHandler: @escaping (_ materialized: Set<NSFileProviderItemIdentifier>, _ evicted: Set<NSFileProviderItemIdentifier>) -> Void) {
        self.account = account
        self.dbManager = dbManager
        logger = FileProviderLogger(category: "MaterializedEnumerationObserver", log: log)
        self.completionHandler = completionHandler
        super.init()
    }

    public func didEnumerate(_ updatedItems: [NSFileProviderItemProtocol]) {
        updatedItems
            .map(\.itemIdentifier)
            .forEach { enumeratedItems.insert($0) }
    }

    public func finishEnumerating(upTo _: NSFileProviderPage?) {
        logger.debug("Handling enumerated materialized items.")
        handleEnumeratedItems(enumeratedItems, account: account, dbManager: dbManager, completionHandler: completionHandler)
    }

    public func finishEnumeratingWithError(_ error: Error) {
        logger.error("Finishing enumeration with error.", [.error: error])
        handleEnumeratedItems(enumeratedItems, account: account, dbManager: dbManager, completionHandler: completionHandler)
    }

    func handleEnumeratedItems(_ identifiers: Set<NSFileProviderItemIdentifier>, account: Account, dbManager: FilesDatabaseManager, completionHandler: @escaping (_ materialized: Set<NSFileProviderItemIdentifier>, _ evicted: Set<NSFileProviderItemIdentifier>) -> Void) {
        let metadataForMaterializedItems = dbManager.materialisedItemMetadatas(account: account.ncKitAccount)
        var metadataForMaterializedItemsByIdentifier = [NSFileProviderItemIdentifier: SendableItemMetadata]()
        var evictedItems = Set<NSFileProviderItemIdentifier>()
        var stillMaterializedItems = Set<NSFileProviderItemIdentifier>()

        for metadata in metadataForMaterializedItems {
            let identifier = NSFileProviderItemIdentifier(metadata.ocId)
            metadataForMaterializedItemsByIdentifier[identifier] = metadata
            evictedItems.insert(identifier) // Assume the item related to the metadata object was evicted until proven otherwise below.
        }

        for enumeratedIdentifier in identifiers {
            if evictedItems.contains(enumeratedIdentifier) {
                evictedItems.remove(enumeratedIdentifier) // The enumerated item cannot be assumed as evicted any longer.
            } else {
                stillMaterializedItems.insert(enumeratedIdentifier)

                guard var metadata = if enumeratedIdentifier == .rootContainer {
                    dbManager.rootItemMetadata(account: account)
                } else {
                    dbManager.itemMetadata(enumeratedIdentifier)
                } else {
                    logger.error("No metadata for enumerated item found.", [.item: enumeratedIdentifier])
                    continue
                }

                if metadata.directory {
                    metadata.visitedDirectory = true
                } else {
                    metadata.downloaded = true
                }

                logger.info("Updating state for item to materialized.", [.item: enumeratedIdentifier, .name: metadata.fileName])
                dbManager.addItemMetadata(metadata)
            }
        }

        for evictedItemIdentifier in evictedItems {
            guard var metadata = metadataForMaterializedItemsByIdentifier[evictedItemIdentifier] else {
                logger.error("No metadata found for apparently evicted identifier.", [.item: evictedItemIdentifier])
                continue
            }

            logger.info("Updating item state to dataless.", [.name: metadata.fileName, .item: evictedItemIdentifier])

            metadata.downloaded = false
            metadata.visitedDirectory = false
            dbManager.addItemMetadata(metadata)
        }

        completionHandler(stillMaterializedItems, evictedItems)
    }
}

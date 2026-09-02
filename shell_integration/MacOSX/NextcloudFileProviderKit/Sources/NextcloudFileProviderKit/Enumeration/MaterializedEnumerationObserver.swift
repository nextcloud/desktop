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

    ///
    /// A failed enumeration must not be reconciled.
    ///
    /// ``handleEnumeratedItems(_:account:dbManager:completionHandler:)`` treats absence from the
    /// enumeration as proof of eviction, so running it over a partial result marks every item the
    /// system never got round to reporting as dataless — the framework then re-downloads them,
    /// and each download triggers another materialized-set enumeration. Reporting nothing leaves
    /// the database untouched and lets the next successful pass reconcile.
    ///
    public func finishEnumeratingWithError(_ error: Error) {
        logger.error("Finishing enumeration with error. Skipping materialized-set reconciliation.", [.error: error])
        completionHandler([], [])
    }

    func handleEnumeratedItems(_ identifiers: Set<NSFileProviderItemIdentifier>, account: Account, dbManager: FilesDatabaseManager, completionHandler: @escaping (_ materialized: Set<NSFileProviderItemIdentifier>, _ evicted: Set<NSFileProviderItemIdentifier>) -> Void) {
        let metadataForMaterializedItems = dbManager.materialisedItemMetadatas(account: account.ncKitAccount)
        var metadataForMaterializedItemsByIdentifier = [NSFileProviderItemIdentifier: SendableItemMetadata]()
        var evictedItems = Set<NSFileProviderItemIdentifier>()
        var stillMaterializedItems = Set<NSFileProviderItemIdentifier>()
        var metadatasToPersist = [SendableItemMetadata]()

        for metadata in metadataForMaterializedItems {
            let identifier = NSFileProviderItemIdentifier(metadata.ocId)
            metadataForMaterializedItemsByIdentifier[identifier] = metadata
            evictedItems.insert(identifier) // Assume the item related to the metadata object was evicted until proven otherwise below.
        }

        for enumeratedIdentifier in identifiers {
            // The system now accounts for this item itself, so the download record that protected
            // it from the reconciliation below has served its purpose.
            PendingMaterializationRegistry.shared.confirmMaterialized(enumeratedIdentifier)

            if evictedItems.contains(enumeratedIdentifier) {
                evictedItems.remove(enumeratedIdentifier) // The enumerated item cannot be assumed as evicted any longer.
            } else {
                stillMaterializedItems.insert(enumeratedIdentifier)

                var metadata: SendableItemMetadata?

                switch enumeratedIdentifier {
                    case .rootContainer:
                        metadata = dbManager.rootItemMetadata(account: account)
                    case .trashContainer:
                        continue // there is no placeholder item for the trash container in the database
                    default:
                        metadata = dbManager.itemMetadata(enumeratedIdentifier)
                }

                guard var metadata else {
                    logger.error("No metadata for enumerated item found.", [.item: enumeratedIdentifier])
                    continue
                }

                if metadata.directory {
                    metadata.visitedDirectory = true
                } else {
                    metadata.downloaded = true
                }

                logger.debug("Updating state for item to materialized.", [.item: enumeratedIdentifier, .name: metadata.fileName])
                metadatasToPersist.append(metadata)
            }
        }

        // An item this process downloaded moments ago is legitimately absent from the system's
        // materialized set until the system catches up with it. Without this the reconciliation
        // flipped every fresh download back to dataless, the framework re-requested the content,
        // and a bulk materialisation never converged. See ``PendingMaterializationRegistry``.
        let unconfirmed = PendingMaterializationRegistry.shared.awaitingConfirmation(among: evictedItems)

        if !unconfirmed.isEmpty {
            logger.debug("Deferring \(unconfirmed.count) recently downloaded item(s) awaiting confirmation from the system.")
            evictedItems.subtract(unconfirmed)
        }

        // Only a file can go dataless. A directory has no payload of its own: `Item.isDownloaded`
        // reports `true` for every directory regardless of the flag, `displayEvict` is hardcoded
        // `false` for directories, and `hasEvictableDescendantFile` counts only `directory == false`
        // rows — so a directory's `downloaded` is never read.
        //
        // Marking one evicted was therefore not just useless but self-perpetuating. A directory
        // qualifies as materialized through `visitedDirectory`, which this reconciliation
        // deliberately preserves (see below), so clearing `downloaded` did nothing to remove it
        // from `materialisedItemMetadatas`. Any visited directory the system does not report back —
        // routine, since macOS lists materialized *content* — was re-evicted on every single pass,
        // forever, each time costing a database write and a log line. Measured on a font library:
        // 630 directories re-evicted across 80 passes, 30,117 transitions and 28,334 log lines in
        // seven minutes, the per-pass count climbing monotonically (122 → 596) as browsing marked
        // more directories visited.
        let evictedFiles = evictedItems.filter { metadataForMaterializedItemsByIdentifier[$0]?.directory == false }
        let skippedDirectories = evictedItems.count - evictedFiles.count

        if skippedDirectories > 0 {
            logger.debug("Ignoring \(skippedDirectories) directory/directories absent from the materialized set; only files carry materialized content.")
        }

        evictedItems = evictedFiles

        for evictedItemIdentifier in evictedItems {
            guard var metadata = metadataForMaterializedItemsByIdentifier[evictedItemIdentifier] else {
                logger.error("No metadata found for apparently evicted identifier.", [.item: evictedItemIdentifier])
                continue
            }

            logger.debug("Updating item state to dataless.", [.name: metadata.fileName, .item: evictedItemIdentifier])

            metadata.downloaded = false
            // Files only reach this loop, and a file's `visitedDirectory` is meaningless — clear it
            // alongside `downloaded` as before. A directory's `visitedDirectory` is our refresh
            // subscription (keep watching a folder Finder has browsed for remote child changes) and
            // is left untouched: directories are filtered out above.
            metadata.visitedDirectory = false

            metadatasToPersist.append(metadata)
        }

        // One transaction for the whole reconciliation, and one log line for the whole pass.
        // This runs synchronously on the framework's callback thread, and it previously opened a
        // write transaction and emitted an `info` line — each of which fsyncs through the single
        // logging actor — for every single item.
        dbManager.addItemMetadatas(metadatasToPersist)

        logger.info("Reconciled materialized set: \(stillMaterializedItems.count) newly materialized, \(evictedItems.count) evicted, \(unconfirmed.count) deferred.")

        completionHandler(stillMaterializedItems, evictedItems)
    }
}

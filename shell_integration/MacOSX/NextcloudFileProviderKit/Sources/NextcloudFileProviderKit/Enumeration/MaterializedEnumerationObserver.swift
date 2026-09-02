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
        var evictionCandidates = Set<NSFileProviderItemIdentifier>()
        var evictedItems = Set<NSFileProviderItemIdentifier>()
        var stillMaterializedItems = Set<NSFileProviderItemIdentifier>()
        var metadatasToPersist = [SendableItemMetadata]()

        for metadata in metadataForMaterializedItems {
            let identifier = NSFileProviderItemIdentifier(metadata.ocId)
            metadataForMaterializedItemsByIdentifier[identifier] = metadata
            evictionCandidates.insert(identifier) // Assume the item related to the metadata object was evicted until proven otherwise below.
        }

        for enumeratedIdentifier in identifiers {
            // The system now accounts for this item itself, so the download record that protected
            // it from the reconciliation below has served its purpose.
            PendingMaterializationRegistry.shared.confirmMaterialized(enumeratedIdentifier)

            if evictionCandidates.contains(enumeratedIdentifier) {
                evictionCandidates.remove(enumeratedIdentifier) // The enumerated item cannot be assumed as evicted any longer.
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
        let unconfirmed = PendingMaterializationRegistry.shared.awaitingConfirmation(among: evictionCandidates)

        if !unconfirmed.isEmpty {
            logger.debug("Deferring \(unconfirmed.count) recently downloaded item(s) awaiting confirmation from the system.")
            evictionCandidates.subtract(unconfirmed)
        }

        for candidateIdentifier in evictionCandidates {
            guard let materializedMetadata = metadataForMaterializedItemsByIdentifier[candidateIdentifier] else {
                logger.error("No metadata found for apparently evicted identifier.", [.item: candidateIdentifier])
                continue
            }

            var metadata = materializedMetadata
            metadata.downloaded = false

            // Being absent from enumeratorForMaterializedItems only means the item has no
            // local materialized content. For directories, visitedDirectory is our refresh
            // subscription: if Finder has enumerated a folder before, keep watching it for
            // remote child changes even when macOS reports it as dataless. This matters for
            // shared mount roots, which may be omitted from materialized items after browsing.
            if !metadata.directory {
                metadata.visitedDirectory = false
            }

            // Persist and report only genuine state transitions, so a file already recorded as
            // dataless is not rewritten on every pass (#10558).
            guard metadata.downloaded != materializedMetadata.downloaded
                || metadata.visitedDirectory != materializedMetadata.visitedDirectory
            else {
                continue
            }

            logger.debug("Updating item state to dataless.", [.name: metadata.fileName, .item: candidateIdentifier])

            metadatasToPersist.append(metadata)
            evictedItems.insert(candidateIdentifier)
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

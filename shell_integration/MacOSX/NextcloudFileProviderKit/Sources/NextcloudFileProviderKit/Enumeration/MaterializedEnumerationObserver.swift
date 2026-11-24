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
    // File Provider can call back inline, so call the enumerator outside this lock.
    private let enumerationLock = NSLock()
    private var enumerator: (any NSFileProviderEnumerator)?
    private var finished = false

    public required init(account: Account, dbManager: FilesDatabaseManager, log: any FileProviderLogging, completionHandler: @escaping (_ materialized: Set<NSFileProviderItemIdentifier>, _ evicted: Set<NSFileProviderItemIdentifier>) -> Void) {
        self.account = account
        self.dbManager = dbManager
        logger = FileProviderLogger(category: "MaterializedEnumerationObserver", log: log)
        self.completionHandler = completionHandler
        super.init()
    }

    /// Lists every page from the system's materialized-item enumerator, then completes once.
    public func enumerateItems(using enumerator: any NSFileProviderEnumerator) {
        let canStart = enumerationLock.withLock {
            guard !finished, self.enumerator == nil else { return false }
            self.enumerator = enumerator
            return true
        }
        guard canStart else { return }
        enumerator.enumerateItems(for: self, startingAt: NSFileProviderPage(Data()))
    }

    public func didEnumerate(_ updatedItems: [NSFileProviderItemProtocol]) {
        let identifiers = updatedItems.map(\.itemIdentifier)
        enumerationLock.withLock {
            guard !finished else { return }
            enumeratedItems.formUnion(identifiers)
        }
    }

    public func finishEnumerating(upTo nextPage: NSFileProviderPage?) {
        if let nextPage {
            let activeEnumerator = enumerationLock.withLock { enumerator }
            activeEnumerator?.enumerateItems(for: self, startingAt: nextPage)
            return
        }
        guard let identifiers = completeEnumeration() else { return }
        logger.debug("Handling enumerated materialized items.")
        handleEnumeratedItems(identifiers, account: account, dbManager: dbManager, completionHandler: completionHandler)
    }

    public func finishEnumeratingWithError(_ error: Error) {
        guard completeEnumeration() != nil else { return }
        logger.error("Finishing enumeration with error.", [.error: error])
        // An incomplete enumeration is not evidence that missing items were evicted.
        // Do not reconcile against a partial snapshot on failure.
        completionHandler([], [])
    }

    private func completeEnumeration() -> Set<NSFileProviderItemIdentifier>? {
        let result = enumerationLock.withLock { () -> (Set<NSFileProviderItemIdentifier>, (any NSFileProviderEnumerator)?)? in
            guard !finished else { return nil }
            finished = true
            let result = (enumeratedItems, enumerator)
            enumeratedItems.removeAll()
            enumerator = nil
            return result
        }
        result?.1?.invalidate()
        return result?.0
    }

    func handleEnumeratedItems(_ identifiers: Set<NSFileProviderItemIdentifier>, account: Account, dbManager: FilesDatabaseManager, completionHandler: @escaping (_ materialized: Set<NSFileProviderItemIdentifier>, _ evicted: Set<NSFileProviderItemIdentifier>) -> Void) {
        let metadataForMaterializedItems = dbManager.materialisedItemMetadatas(account: account.ncKitAccount)
        var metadataForMaterializedItemsByIdentifier = [NSFileProviderItemIdentifier: SendableItemMetadata]()
        var evictionCandidates = Set<NSFileProviderItemIdentifier>()
        var evictedItems = Set<NSFileProviderItemIdentifier>()
        var stillMaterializedItems = Set<NSFileProviderItemIdentifier>()
        var materializationStates = [(ocId: String, downloaded: Bool?, visitedDirectory: Bool?)]()

        for metadata in metadataForMaterializedItems {
            let identifier = NSFileProviderItemIdentifier(metadata.ocId)
            metadataForMaterializedItemsByIdentifier[identifier] = metadata
            evictionCandidates.insert(identifier) // Assume the item related to the metadata object was evicted until proven otherwise below.
        }

        for enumeratedIdentifier in identifiers {
            // The system now accounts for this item itself, so the download record that protected
            // it from the reconciliation below has served its purpose.
            dbManager.pendingMaterializations.confirmMaterialized(enumeratedIdentifier)

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

                guard let metadata else {
                    logger.error("No metadata for enumerated item found.", [.item: enumeratedIdentifier])
                    continue
                }

                logger.debug("Updating state for item to materialized.", [.item: enumeratedIdentifier, .name: metadata.fileName])

                if metadata.directory {
                    materializationStates.append((ocId: metadata.ocId, downloaded: nil, visitedDirectory: true))
                } else {
                    materializationStates.append((ocId: metadata.ocId, downloaded: true, visitedDirectory: nil))
                }
            }
        }

        // An item this process downloaded moments ago is legitimately absent from the system's
        // materialized set until the system catches up, so it must not be reconciled as evicted
        // yet — see `PendingMaterializationRegistry`.
        let unconfirmed = dbManager.pendingMaterializations.awaitingConfirmation(among: evictionCandidates)

        evictionCandidates.subtract(unconfirmed)

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

            materializationStates.append((ocId: metadata.ocId, downloaded: false, visitedDirectory: metadata.directory ? nil : false))
            evictedItems.insert(candidateIdentifier)
        }

        // One transaction for the whole pass, which runs synchronously on the framework's callback
        // thread, writing only the `downloaded` and `visitedDirectory` flags so a row that changed
        // since this pass read it, for example by a download finishing meanwhile, keeps its other
        // columns.
        dbManager.setMaterializationStates(materializationStates)

        logger.info("Reconciled materialized set.", [.materializedCount: stillMaterializedItems.count, .evictedCount: evictedItems.count, .deferredCount: unconfirmed.count])

        completionHandler(stillMaterializedItems, evictedItems)
    }
}

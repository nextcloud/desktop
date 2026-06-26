//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// Working-set change derivation. Because a change notification only ever signals `.workingSet`,
/// this is the path that drives remote changes into the framework for items the system tracks
/// (visited folders and downloaded files) and the subtrees they reach.
///
extension Enumerator {
    ///
    /// Derive and report the working-set changes since `date`.
    ///
    /// Combines the changes discovered by scanning the materialised items directly against the
    /// server (``scanMaterialisedItemsForRemoteChanges()``) with the pending local changes
    /// re-derived from the database (``FilesDatabaseManager/pendingWorkingSetChanges(since:)``). The
    /// server scan is what surfaces remote changes the database-only reconstruction misses:
    /// non-materialised items, and items whose own parent directory did not itself change.
    ///
    func enumerateWorkingSetChanges(for observer: NSFileProviderChangeObserver, since date: Date) {
        logger.debug("Enumerating changes in working set.", [.account: account])

        Task {
            let serverChanges = await scanMaterialisedItemsForRemoteChanges()
            let pendingLocalChanges = dbManager.pendingWorkingSetChanges(since: date)

            let changes = ChangeSet(
                mergingUpdated: [serverChanges.updated, pendingLocalChanges.updated],
                deleted: [serverChanges.deleted, pendingLocalChanges.deleted]
            )

            completeChangesObserver(
                observer,
                anchor: currentAnchor,
                enumeratedItemIdentifier: enumeratedItemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                changes: changes
            )
        }
    }

    ///
    /// Scan the materialised items (and any changed subtrees they reveal) on the server, persist the
    /// discovered changes to the database, and return them.
    ///
    /// The returned changes are reported to the working-set change observer directly. Relying on
    /// ``FilesDatabaseManager/pendingWorkingSetChanges(since:)`` alone to re-derive the report from
    /// the database loses changes to non-materialised items and to items in subtrees whose own parent
    /// directory did not change, because that reconstruction is gated on the materialised set and
    /// `syncTime`.
    ///
    /// - Returns: The discovered creations and updates (combined into `updated`) and the items that
    ///   were marked deleted.
    ///
    func scanMaterialisedItemsForRemoteChanges() async -> (
        updated: [SendableItemMetadata], deleted: [SendableItemMetadata]
    ) {
        logger.debug("Checking materialised items for changes on the server...")

        defer {
            logger.debug("Completed checking materialised items for changes on the server.")
        }

        // Unlike when enumerating items we can't progressively enumerate items as we need to
        // wait to see which items are truly deleted and which have just been moved elsewhere.
        // Visited folders and downloaded files. Sort in terms of their remote URLs.
        // This way we ensure we visit parent folders before their children.
        let materialisedItems = dbManager
            .materialisedItemMetadatas(account: account.ncKitAccount)
            .filter { !$0.deleted }
            .sorted { $0.remotePath().count < $1.remotePath().count }

        var accumulatedCreations = [SendableItemMetadata]()
        var accumulatedUpdates = [SendableItemMetadata]()
        var accumulatedDeletions = [SendableItemMetadata]()
        var scannedItemIds = Set<String>()

        // Work queue seeded with the materialised items. A changed child directory discovered while
        // scanning is appended ONLY when its subtree actually contains a materialised item, so its
        // changed descendants are visited too — otherwise a depth-1 read of a visited folder surfaces
        // the changed subdirectory but never the changed items inside it. A changed subdirectory whose
        // subtree holds nothing materialised is NOT enqueued: nothing inside it is part of the working
        // set, so there is nothing to keep in sync, and its contents are read lazily when the user
        // navigates into it. Without that bound a single working-set signal on a sparse / freshly
        // activated domain triggers a full recursive PROPFIND of every changed branch down to its
        // leaves (every never-enumerated descendant looks "new"), hammering the server. Unchanged
        // subtrees are likewise never enqueued, so the "skip unchanged directories" optimisation holds.
        var scanQueue = materialisedItems
        var enqueuedDirectoryIds = Set(materialisedItems.filter(\.directory).map(\.ocId))
        var scanIndex = 0

        while scanIndex < scanQueue.count {
            let itemToScan = scanQueue[scanIndex]
            scanIndex += 1

            guard !scannedItemIds.contains(itemToScan.ocId) else { continue }
            guard isLockFileName(itemToScan.fileName) == false else {
                // Skip server requests for locally created lock files.
                // They are not synchronised to the server for real.
                // Thus they can be expected not to be found there.
                // That would also cause their local deletion due to synchronisation logic.
                logger.debug("Skipping materialised item in working set check because the name hints a lock file.", [.item: itemToScan, .name: itemToScan.name])
                continue
            }

            let itemRemoteUrl = itemToScan.remotePath()

            let readResult = await Enumerator.readServerUrl(itemRemoteUrl, account: account, remoteInterface: remoteInterface, dbManager: dbManager, depth: itemToScan.directory ? .targetAndDirectChildren : .target, log: logger.log)
            let changes = readResult.changes ?? ChangeSet()

            if readResult.error?.errorCode == 404 {
                accumulatedDeletions.append(itemToScan)
                scannedItemIds.insert(itemToScan.ocId)
                // Children are not marked deleted here — they may have moved with their parent.
                logger.debug("Parent returned 404; children will be checked individually.", [.url: itemRemoteUrl])
            } else if let readError = readResult.error, readError != .success {
                logger.error("Finished remote change enumeration of materialised items with error.", [.error: readError])
                // Report what was discovered before the error rather than discarding it.
                break
            } else {
                accumulatedDeletions += changes.deleted
                accumulatedUpdates += changes.updated
                accumulatedCreations += changes.created

                // Reading a directory's children does not by itself require scanning each child's own
                // children. Track which children this read has already accounted for.
                var childrenCoveredByThisRead = Set<String>()

                if let readItems = readResult.metadatas, let readTarget = readItems.first {
                    scannedItemIds.insert(readTarget.ocId)

                    if readItems.count > 1 {
                        childrenCoveredByThisRead.formUnion(readItems[1...].filter { !$0.directory }.map(\.ocId))
                    }

                    if readItems.count > 1 {
                        let childDirectories = readItems[1...].filter(\.directory)
                        let changedChildOcIds = Set(changes.updated.map(\.ocId))
                            .union(changes.created.map(\.ocId))

                        for childDirectory in childDirectories {
                            // A changed child directory is scanned so its changed descendants are
                            // discovered, even when the directory itself is not materialised — but
                            // only when the working set actually tracks something inside it, i.e. it
                            // has a materialised descendant (a visited subfolder or a downloaded file).
                            // A changed-but-unmaterialised subtree is never crawled here: nothing in
                            // it is cached locally, so its contents are read lazily on navigation
                            // rather than walked now (which on a sparse domain would recurse into
                            // entire never-visited subtrees). Its own change is still reported above
                            // via `accumulatedUpdates` / `accumulatedCreations`.
                            if changedChildOcIds.contains(childDirectory.ocId) {
                                let childPath = childDirectory.remotePath()
                                let childHasMaterialisedDescendant = materialisedItems.contains {
                                    $0.ocId != childDirectory.ocId
                                        && ($0.serverUrl == childPath
                                            || $0.serverUrl.hasPrefix(childPath + "/"))
                                }
                                if childHasMaterialisedDescendant,
                                   enqueuedDirectoryIds.insert(childDirectory.ocId).inserted
                                {
                                    scanQueue.append(childDirectory)
                                }
                                continue
                            }

                            // Only skip unchanged child directories with no materialised descendants.
                            // Lock changes don't propagate etags, so dirs with visible children must be enumerated.
                            guard let localItem = materialisedItems.first(
                                where: { $0.ocId == childDirectory.ocId }
                            ), localItem.isInSameDatabaseStoreableRemoteState(childDirectory) else {
                                continue
                            }

                            let hasMaterialisedDescendants = materialisedItems.contains {
                                $0.ocId != localItem.ocId
                                    && $0.serverUrl.hasPrefix(localItem.remotePath() + "/")
                            }

                            if !hasMaterialisedDescendants {
                                childrenCoveredByThisRead.insert(childDirectory.ocId)
                            }
                        }
                    }

                    childrenCoveredByThisRead.formUnion(changes.deleted.map(\.ocId))
                }

                scannedItemIds.formUnion(childrenCoveredByThisRead)
            }
        }

        // Catches moves across directories: items found at a new location (updated or new)
        // should not be marked deleted at the old location.
        let survivingOcIds = Set(accumulatedUpdates.map(\.ocId))
            .union(accumulatedCreations.map(\.ocId))

        accumulatedDeletions.removeAll { survivingOcIds.contains($0.ocId) }

        var reportedDeletions = [SendableItemMetadata]()
        for deletedMetadata in accumulatedDeletions {
            if deletedMetadata.status >= Status.inUpload.rawValue {
                logger.info("Skipping deletion of item with pending upload.", [.item: deletedMetadata.ocId])
                continue
            }
            var deleteMarked = deletedMetadata
            deleteMarked.deleted = true
            deleteMarked.syncTime = Date()
            dbManager.addItemMetadata(deleteMarked)
            reportedDeletions.append(deleteMarked)
        }

        // Deduplicate the discovered new/updated metadata by ocId, preserving order.
        var seenUpdatedOcIds = Set<String>()
        let discoveredUpdates = (accumulatedCreations + accumulatedUpdates).filter {
            seenUpdatedOcIds.insert($0.ocId).inserted
        }

        if discoveredUpdates.isEmpty, reportedDeletions.isEmpty {
            logger.info("No remote changes found in materialised items.")
        }

        return (updated: discoveredUpdates, deleted: reportedDeletions)
    }
}

//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// How many working-set scan reads may be in flight at once, equal to NextcloudKit's default per-host connection limit on the data session that downloads and uploads share, so on an HTTP/1.1 server a transfer started during a scan waits for a scan read to finish.
///
private let workingSetScanConcurrency = 6

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
    /// server (``scanMaterialisedItemsForRemoteChanges(startingFrom:)``) with the pending local changes
    /// re-derived from the database (``FilesDatabaseManager/pendingWorkingSetChanges(since:)``). The
    /// server scan is what surfaces remote changes the database-only reconstruction misses:
    /// non-materialised items, and items whose own parent directory did not itself change.
    ///
    func enumerateWorkingSetChanges(
        for observer: NSFileProviderChangeObserver, since date: Date, anchor: NSFileProviderSyncAnchor
    ) {
        logger.debug("Enumerating changes in working set.", [.account: account])

        let anchorKey = String(data: anchor.rawValue, encoding: .utf8) ?? ""

        Task {
            // Derive the change set once per drain sequence. The scan is destructive — it persists its
            // discoveries to the database as it recurses — so on the framework's moreComing
            // re-invocations we must drain the buffer rather than re-derive, or every change beyond the
            // first batch is silently dropped.
            let isPrimed = readDeliveryState(reportingTo: observer) {
                try changeBuffer.isPrimed(forKey: anchorKey)
            }

            guard let isPrimed else {
                return
            }

            if !isPrimed {
                // A re-invocation under a *different* anchor is a fresh enumeration; drop any stale
                // remainder before deriving anew.
                changeBuffer.reset()
                logger.debug("Working-set change buffer not primed for anchor \(anchorKey); deriving changes.", [.account: account])

                let scanStarted = Date()
                let scope = remoteChangeTargets.takeScope(now: scanStarted)
                var scans = switch scope {
                    case .fullWalk:
                        await [scanMaterialisedItemsForRemoteChanges()]
                    case let .targets(targets):
                        await [scanMaterialisedItemsForRemoteChanges(startingFrom: targets)]
                }

                if scans[0].needsFullWalk {
                    let fullWalk = await scanMaterialisedItemsForRemoteChanges()

                    // A deletion always wins the merge below, so one that the full walk disproved is dropped here.
                    let foundOcIds = Set(fullWalk.updated.filter { !$0.deleted }.map(\.ocId))
                    scans[0].deleted.removeAll { foundOcIds.contains($0.ocId) }

                    // The full walk goes first so its state of an item both scans report wins the merge below.
                    scans.insert(fullWalk, at: 0)
                }

                let serverHadFailure = scans.contains(where: \.hadFailure)

                if scans.contains(where: \.materialisedItemsUnreadable) {
                    // The materialised items could not be read, so nothing is known about what this scan missed.
                    remoteChangeTargets.requestFullWalk()
                } else {
                    // A failed read is retried on its own, so one folder that keeps failing does not turn every scan into a full walk.
                    remoteChangeTargets.record(targets: scans.flatMap(\.failedOcIds).map { NSFileProviderItemIdentifier($0) })

                    if scope == .fullWalk || scans.count > 1 {
                        remoteChangeTargets.noteFullWalkCompleted(startedAt: scanStarted)
                    }
                }

                // A failed local read is treated like a failed server scan: the incoming anchor is kept so the changes are retried on the next signal.
                let localChanges = dbManager.pendingWorkingSetChanges(since: date)
                let pendingLocalChanges = localChanges ?? ([], [])
                let hadFailure = serverHadFailure || localChanges == nil

                let changes = ChangeSet(
                    mergingUpdated: scans.map(\.updated) + [pendingLocalChanges.updated],
                    deleted: scans.map(\.deleted) + [pendingLocalChanges.deleted]
                )

                // Sort created+updated by remote-path length (ascending) so parent directories are
                // reported before their children. Draining the single combined list front-to-back
                // preserves this across batches; without it macOS may create a rename-destination folder
                // to house a child before it processes the parent rename, briefly leaving both the old
                // and new folder names on disk.
                // Paths of equal length are ordered by path, so the order does not depend on which scan read finished first.
                let sortedUpdated = changes.createdAndUpdated
                    .sorted { ($0.remotePath().count, $0.remotePath()) < ($1.remotePath().count, $1.remotePath()) }

                let finalAnchor = hadFailure ? anchor : currentAnchor
                changeBuffer.prime(
                    key: anchorKey,
                    finalAnchorRawValue: finalAnchor.rawValue,
                    updated: sortedUpdated,
                    deleted: changes.deleted,
                    incomplete: hadFailure
                )
            }

            // Intermediate batches use continuation anchors. If the scan was incomplete, keep the
            // incoming anchor for the final batch so missed changes are retried on the next signal.
            let incomplete = readDeliveryState(reportingTo: observer) {
                try changeBuffer.isPrimedIncomplete()
            }

            guard let incomplete else {
                return
            }

            let finalAnchor = incomplete ? anchor : currentAnchor
            drainChangeBuffer(
                for: observer,
                finalAnchor: finalAnchor,
                suggested: observer.suggestedBatchSize
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
    /// - Parameter targets: The items to start the walk from, or `nil` for all materialised items.
    /// - Returns: What the scan found, read and failed to read.
    ///
    func scanMaterialisedItemsForRemoteChanges(
        startingFrom targets: Set<NSFileProviderItemIdentifier>? = nil
    ) async -> WorkingSetScanResult {
        logger.debug("Checking materialised items for changes on the server...")

        defer {
            logger.debug("Completed checking materialised items for changes on the server.")
        }

        // Trashed rows are excluded because trashing moves `serverUrl` to the trashbin without
        // setting `deleted`, and the ordinary DAV path 404s there — which the scan reads as a
        // deletion of the row trash reconciliation needs.
        // Unlike when enumerating items we can't progressively enumerate items as we need to
        // wait to see which items are truly deleted and which have just been moved elsewhere.
        // Visited folders and downloaded files. Sort in terms of their remote URLs.
        // This way we ensure we visit parent folders before their children.
        // A failed read counts as a read failure of the whole scan, so the sync point does not advance past changes this pass could not see.
        let storedMaterialisedItems = dbManager.materialisedItemMetadatas(account: account.ncKitAccount)
        let materialisedItems = (storedMaterialisedItems ?? [])
            .filter { !$0.deleted && !$0.isTrashed }
            .sorted { $0.remotePath().count < $1.remotePath().count }

        var accumulatedCreations = [SendableItemMetadata]()
        var accumulatedUpdates = [SendableItemMetadata]()
        var accumulatedDeletions = [SendableItemMetadata]()
        var scannedItemIds = Set<String>()
        // Track read failures so one unreadable folder no longer aborts the whole scan (see the
        // read-error branch below). `hadReadFailure` is returned to the caller so it can avoid
        // advancing the working-set sync point past changes this pass could not discover.
        var hadReadFailure = storedMaterialisedItems == nil
        var failedItemIds = Set<String>()
        var notFoundOcIds = Set<String>()

        // Reads are issued in waves by remote-path depth, so a directory's read is merged before the deeper items it covers are read.
        func remotePathDepth(_ metadata: SendableItemMetadata) -> Int {
            metadata.remotePath().count(where: { $0 == "/" })
        }

        // Work queue seeded with the materialised items. A changed child directory discovered while
        // scanning is appended ONLY when its subtree actually contains a materialised item or the
        // directory is pinned, so its changed descendants are visited too — otherwise a depth-1 read of
        // a visited folder surfaces the changed subdirectory but never the changed items inside it. A
        // changed, unpinned subdirectory whose subtree holds nothing materialised is NOT enqueued:
        // nothing inside it is part of the working set, so there is nothing to keep in sync, and its
        // contents are read lazily when the user navigates into it. Without that bound a single working-set signal on a sparse / freshly
        // activated domain triggers a full recursive PROPFIND of every changed branch down to its
        // leaves (every never-enumerated descendant looks "new"), hammering the server. Unchanged
        // subtrees are likewise never enqueued, so the "skip unchanged directories" optimisation holds.
        // `materialisedItems` stays the full set below, because the descendant checks consult it; only the seed of the walk narrows.
        var seedItems = materialisedItems
        if let targets {
            let targetOcIds = Set(targets.map(\.rawValue))
            seedItems = materialisedItems.filter { targetOcIds.contains($0.ocId) }

            // A target folder that is not materialised is read when it would be queued as a changed child, so its listing can show where a materialised item moved.
            let materialisedOcIds = Set(materialisedItems.map(\.ocId))
            for ocId in targetOcIds.subtracting(materialisedOcIds) {
                guard let folder = dbManager.itemMetadata(ocId: ocId), folder.directory, !folder.deleted, !folder.isTrashed else {
                    continue
                }

                if folder.keepDownloaded || materialisedItems.contains(where: { $0.isDescendant(of: folder.remotePath()) }) {
                    seedItems.append(folder)
                }
            }

            logger.info("Scanning the items pushes named instead of the whole materialised set.", [.targetCount: seedItems.count])
        } else {
            logger.info("Scanning the whole materialised set.", [.targetCount: seedItems.count])
        }
        var seededOcIds = Set(seedItems.map(\.ocId))
        var scanQueue = Dictionary(grouping: seedItems, by: remotePathDepth)
        let materialisedDirectoryIds = Set(materialisedItems.filter(\.directory).map(\.ocId))
        var enqueuedDirectoryIds = Set(seedItems.filter(\.directory).map(\.ocId))

        // Items a successful read listed, which exist on the server whatever their own read at an older path returned.
        var listedOcIds = Set<String>()

        // Paths of the directories this scan read successfully, or whose deletion a successful read listed.
        var confirmedDirectoryPaths = Set<String>()

        // A targeted scan reads the materialised items below a folder that is gone, which a full walk reads as its own starting points.
        func queueMaterialisedItems(below path: String) {
            for descendant in materialisedItems where descendant.isDescendant(of: path) && seededOcIds.insert(descendant.ocId).inserted {
                scanQueue[remotePathDepth(descendant), default: []].append(descendant)
            }
        }

        while let waveDepth = scanQueue.keys.min() {
            let wave = scanQueue.removeValue(forKey: waveDepth) ?? []

            let toRead = wave.filter { candidate in
                guard !scannedItemIds.contains(candidate.ocId) else { return false }
                guard isLockFileName(candidate.fileName) == false else {
                    // Skip server requests for locally created lock files.
                    // They are not synchronised to the server for real.
                    // Thus they can be expected not to be found there.
                    // That would also cause their local deletion due to synchronisation logic.
                    logger.debug("Skipping materialised item in working set check because the name hints a lock file.", [.item: candidate, .name: candidate.name])
                    return false
                }
                return true
            }

            guard !toRead.isEmpty else { continue }

            await concurrentlyReadForWorkingSetScan(toRead, checkingIdentity: targets != nil) { itemToScan, readResult in
                // A sibling merged earlier in this wave covers this item when the server moved it into that sibling or onto its path.
                guard !scannedItemIds.contains(itemToScan.ocId) else { return }

                let itemRemoteUrl = itemToScan.remotePath()
                let changes = readResult.changes ?? ChangeSet()

                // In a scan that checks identity, a path that now holds a different folder means the item is not there, like a 404.
                if readResult.error?.errorCode == 404 || (targets != nil && readResult.error == .invalidData) {
                    accumulatedDeletions.append(itemToScan)
                    notFoundOcIds.insert(itemToScan.ocId)
                    scannedItemIds.insert(itemToScan.ocId)
                    // Children are not marked deleted here — they may have moved with their parent.
                    logger.debug("Parent returned 404; children will be checked individually.", [.url: itemRemoteUrl])
                } else if let readError = readResult.error, readError != .success {
                    // A single unreadable folder must NOT abort discovery for the rest of the working set.
                    // The queue is sorted parent-first, so the account root and top-level folders (e.g. a
                    // very large "Talk" whose depth-1 PROPFIND times out) are scanned first; a `break` here
                    // meant one early failure silently stalled ALL remote-change propagation for every other
                    // folder — the "files uploaded on the web never appear" bug. Skip only the failing item
                    // (it is retried on the next scan) and keep scanning the remainder.
                    // See nextcloud/desktop#10442.
                    logger.error(
                        "Read of materialised item failed during working-set scan; skipping it and continuing with the rest of the working set.",
                        [.error: readError, .url: itemRemoteUrl]
                    )
                    hadReadFailure = true
                    failedItemIds.insert(itemToScan.ocId)
                    scannedItemIds.insert(itemToScan.ocId)
                    return
                } else {
                    accumulatedDeletions += changes.deleted
                    accumulatedUpdates += changes.updated
                    accumulatedCreations += changes.created

                    // Reading a directory's children does not by itself require scanning each child's own
                    // children. Track which children this read has already accounted for.
                    var childrenCoveredByThisRead = Set<String>()

                    if itemToScan.directory {
                        confirmedDirectoryPaths.insert(itemRemoteUrl.precomposedStringWithCanonicalMapping)
                    }

                    for goneDirectory in changes.deleted where targets != nil && goneDirectory.directory {
                        confirmedDirectoryPaths.insert(goneDirectory.remotePath().precomposedStringWithCanonicalMapping)
                        queueMaterialisedItems(below: goneDirectory.remotePath())
                    }

                    if let readItems = readResult.metadatas, let readTarget = readItems.first {
                        listedOcIds.formUnion(readItems.map(\.ocId))
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
                                // has a materialised descendant (a visited subfolder or a downloaded file),
                                // or when it is pinned, because "Always keep downloaded" asks for the whole
                                // subtree. Any other changed-but-unmaterialised subtree is never crawled
                                // here: nothing in it is cached locally, so its contents are read lazily on
                                // navigation rather than walked now (which on a sparse domain would recurse
                                // into entire never-visited subtrees). Its own change is still reported
                                // above via `accumulatedUpdates` / `accumulatedCreations`.
                                if changedChildOcIds.contains(childDirectory.ocId) {
                                    let childPath = childDirectory.remotePath()
                                    let childHasMaterialisedDescendant = materialisedItems.contains {
                                        $0.ocId != childDirectory.ocId
                                            && ($0.hasSameRemotePath(as: childPath)
                                                || $0.isDescendant(of: childPath))
                                    }
                                    // The pin is read from the stored row because the read applies the parent's pin to its own copy.
                                    let childIsPinned = dbManager
                                        .itemMetadata(ocId: childDirectory.ocId)?
                                        .keepDownloaded == true

                                    // A materialised child is queued even when it is not a starting point, so a targeted scan does not store its new etag without reading it.
                                    if materialisedDirectoryIds.contains(childDirectory.ocId) || childHasMaterialisedDescendant || childIsPinned,
                                       enqueuedDirectoryIds.insert(childDirectory.ocId).inserted
                                    {
                                        scanQueue[remotePathDepth(childDirectory), default: []].append(childDirectory)
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
                                        && $0.isDescendant(of: localItem.remotePath())
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
        }

        // Catches moves across directories: items found at a new location (updated or new)
        // should not be marked deleted at the old location.
        let survivingOcIds = Set(accumulatedUpdates.map(\.ocId))
            .union(accumulatedCreations.map(\.ocId))
            .union(listedOcIds)

        accumulatedDeletions.removeAll { survivingOcIds.contains($0.ocId) }

        // A walk that did not read everything only deletes a missing item when the listing of its parent, or the parent's own deletion, confirms it.
        var needsFullWalk = false
        if targets != nil {
            var confirmedPaths = confirmedDirectoryPaths
            var unconfirmedOcIds = Set<String>()

            for missing in accumulatedDeletions.filter({ notFoundOcIds.contains($0.ocId) }).sorted(by: { $0.remotePath().count < $1.remotePath().count }) {
                if confirmedPaths.contains(missing.serverUrl.precomposedStringWithCanonicalMapping) {
                    confirmedPaths.insert(missing.remotePath().precomposedStringWithCanonicalMapping)
                } else {
                    unconfirmedOcIds.insert(missing.ocId)
                }
            }

            if !unconfirmedOcIds.isEmpty {
                logger.info("Leaving items that were not found to a full walk, because no read showed whether they were deleted or moved.")
                accumulatedDeletions.removeAll { unconfirmedOcIds.contains($0.ocId) }
                needsFullWalk = true
            }
        }

        var reportedDeletions = [SendableItemMetadata]()
        for deletedMetadata in accumulatedDeletions {
            // Check and write in one transaction, so a move that commits in between is not overwritten.
            let deleteMarked: SendableItemMetadata? = dbManager.write("Could not mark item deleted.", [.item: deletedMetadata.ocId]) { db in
                let current = try dbManager.itemMetadata(ocId: deletedMetadata.ocId, in: db)?.metadata

                if (current ?? deletedMetadata).status >= Status.inUpload.rawValue {
                    logger.info("Skipping deletion of item with pending upload.", [.item: deletedMetadata.ocId])
                    return nil
                }

                // A move that reached the database during the scan makes a 404 about the old path, not the item.
                if notFoundOcIds.contains(deletedMetadata.ocId), let current, !current.hasSameRemotePath(as: deletedMetadata.remotePath()) {
                    logger.debug("Ignoring deletion of an item that has since moved.", [.item: deletedMetadata.ocId])
                    return nil
                }

                var deleteMarked = deletedMetadata
                deleteMarked.deleted = true
                deleteMarked.syncTime = Date()
                try dbManager.insertItemMetadata(deleteMarked, in: db)
                return deleteMarked
            } ?? nil

            if let deleteMarked {
                reportedDeletions.append(deleteMarked)
            }
        }

        // Deduplicate the discovered new/updated metadata by ocId, preserving order.
        var seenUpdatedOcIds = Set<String>()
        let discoveredUpdates = (accumulatedCreations + accumulatedUpdates).filter {
            seenUpdatedOcIds.insert($0.ocId).inserted
        }

        if discoveredUpdates.isEmpty, reportedDeletions.isEmpty {
            logger.info("No remote changes found in materialised items.")
        }

        if hadReadFailure {
            logger.error(
                "Working-set remote-change scan was incomplete: \(failedItemIds.count) item(s) could not be read and were skipped; their changes will be retried on the next scan. Not advancing the working-set sync point past the undiscovered changes.",
                [.account: account]
            )
        }

        return WorkingSetScanResult(
            updated: discoveredUpdates,
            deleted: reportedDeletions,
            hadFailure: hadReadFailure,
            failedOcIds: failedItemIds,
            materialisedItemsUnreadable: storedMaterialisedItems == nil,
            needsFullWalk: needsFullWalk
        )
    }

    /// Read `items` from the server, at most ``workingSetScanConcurrency`` at a time, handing each result to `merge` as it arrives.
    ///
    /// - Parameter checkingIdentity: Whether a directory read fails with `.invalidData` when its path now holds a different folder.
    ///
    private func concurrentlyReadForWorkingSetScan(
        _ items: [SendableItemMetadata],
        checkingIdentity: Bool,
        merge: (SendableItemMetadata, RemoteReadResult) -> Void
    ) async {
        await withTaskGroup(of: (SendableItemMetadata, RemoteReadResult).self) { group in
            for (index, item) in items.enumerated() {
                if index >= workingSetScanConcurrency, let (readItem, result) = await group.next() {
                    merge(readItem, result)
                }

                group.addTask {
                    let result = await Enumerator.readServerUrl(
                        item.remotePath(),
                        account: self.account,
                        remoteInterface: self.remoteInterface,
                        dbManager: self.dbManager,
                        expectedDirectoryOcId: checkingIdentity && item.directory ? item.ocId : nil,
                        depth: item.directory ? .targetAndDirectChildren : .target,
                        log: self.logger.log
                    )
                    return (item, result)
                }
            }

            for await (readItem, result) in group {
                merge(readItem, result)
            }
        }
    }
}

//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// How many working-set scan reads may be in flight at once.
///
/// The scan is pure network waiting — a measured pass spent 76 seconds on 669 sequential PROPFINDs
/// of roughly 100ms each to surface zero changes. Six matches the concurrency the framework already
/// drives for content fetches on this domain, so it adds no load pattern the server does not
/// already see.
///
private let workingSetScanConcurrency = 6

///
/// Identifiers a streaming scan has already handed to the change buffer, so the tail append skips
/// them. The wave callback is `@Sendable` and runs on the scan's task, hence the lock.
///
private final class StreamedIdentifiers: @unchecked Sendable {
    private let lock = NSLock()
    private var storage = Set<String>()

    func formUnion(_ ocIds: some Sequence<String>) {
        lock.lock()
        defer { lock.unlock() }
        storage.formUnion(ocIds)
    }

    func contains(_ ocId: String) -> Bool {
        lock.lock()
        defer { lock.unlock() }
        return storage.contains(ocId)
    }
}

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
            if !changeBuffer.isPrimed(forKey: anchorKey) {
                // A re-invocation under a *different* anchor is a fresh enumeration; drop any stale
                // remainder before deriving anew.
                changeBuffer.reset()
                logger.debug("Working-set change buffer not primed for anchor \(anchorKey); deriving changes.", [.account: account])

                // Prefer the containers a push named. Push identifies a change within a second,
                // whereas the full walk grows with the materialised set — 305 seconds when this was
                // measured. The full walk still runs when nothing is targeted, and is forced every
                // `fullScanInterval` regardless, because push can drop messages across reconnects
                // and only ever tells us what *did* change. See ``RemoteChangeTargets``.
                let targets = RemoteChangeTargets.shared.consumeTargets()
                let runFullScan = targets == nil || RemoteChangeTargets.shared.shouldRunFullScan()

                // Stream the walk's discoveries rather than holding them until it ends. A full walk
                // grows with the materialised set — 305 seconds when last measured — and a change
                // found in its first seconds used to wait for its last: one document was seen 15
                // seconds in and reported 4m34s later.
                //
                // Only scan-discovered creations and updates stream. Deletions cannot: an item
                // absent from one directory may have moved into another the walk has not reached,
                // which is only decidable once it has. The database-derived half is read after the
                // scan (the scan's own writes feed it), so it joins the tail too.
                guard let token = changeBuffer.primeStreaming(
                    key: anchorKey, incomingAnchorRawValue: anchor.rawValue
                ) else {
                    logger.error("Could not open a change delivery session; skipping this derivation.")
                    return
                }

                // Detached: awaiting the walk here would hold this call for its whole duration and
                // streaming would buy nothing. The drain below waits only for the first wave, and
                // the framework's continuation calls drain whatever has landed since — from this
                // enumerator or any other, because the session and the producer's liveness are both
                // visible to all of them.
                Task { [changeBuffer] in
                    let streamed = StreamedIdentifiers()
                    let serverChanges = await scanMaterialisedItemsForRemoteChanges(
                        restrictedToContainers: runFullScan ? nil : targets
                    ) { discovered in
                        streamed.formUnion(discovered.map(\.ocId))
                        changeBuffer.append(token: token, updated: discovered, deleted: [])
                    }

                    if runFullScan, !serverChanges.hadFailure {
                        RemoteChangeTargets.shared.noteFullScanCompleted()
                    }

                    let pendingLocalChanges = dbManager.pendingWorkingSetChanges(since: date)

                    let changes = ChangeSet(
                        mergingUpdated: [serverChanges.updated, pendingLocalChanges.updated],
                        deleted: [serverChanges.deleted, pendingLocalChanges.deleted]
                    )

                    // Sort created+updated by remote-path length (ascending) so parent directories
                    // are reported before their children. Draining front-to-back preserves this
                    // across batches; without it macOS may create a rename-destination folder to
                    // house a child before it processes the parent rename, briefly leaving both the
                    // old and new folder names on disk. The streamed portion already satisfies the
                    // rule by construction — waves run shallowest-depth-first — so only the tail
                    // needs sorting.
                    let remainingUpdated = changes.createdAndUpdated
                        .filter { !streamed.contains($0.ocId) }
                        .sorted { $0.remotePath().count < $1.remotePath().count }

                    changeBuffer.append(token: token, updated: remainingUpdated, deleted: changes.deleted)

                    let finalAnchor = serverChanges.hadFailure ? anchor : currentAnchor
                    changeBuffer.finishStreaming(
                        token: token,
                        finalAnchorRawValue: finalAnchor.rawValue,
                        incomplete: serverChanges.hadFailure
                    )
                }
            }

            // Park until the producer has something, so a streaming session never answers with an
            // empty batch and `moreComing: true` — which the framework answers by calling straight
            // back. Returns at once when no producer is running.
            await changeBuffer.awaitDeliverableItems()

            // Intermediate batches use a durable continuation anchor. The final batch normally advances
            // the working-set sync point to
            // currentAnchor — but when the scan was incomplete (a remote read failed and was skipped) we
            // keep the incoming anchor instead, so we do not tell the framework we are synced up to "now"
            // past changes we could not discover this pass. The next working-set signal re-derives and
            // picks up the previously-unreadable folders once they succeed. (isPrimedIncomplete() is read
            // before the final takeBatch clears the buffer, so it reflects this drain sequence.)
            let finalAnchor = changeBuffer.isPrimedIncomplete() ? anchor : currentAnchor
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
    /// - Returns: The discovered creations and updates (combined into `updated`) and the items that
    ///   were marked deleted.
    ///
    /// - Parameter restrictedToContainers: When set, seed the walk with only these containers
    ///   instead of the whole materialised set — the containers a `notify_push` named. Subtree
    ///   discovery, coverage and deletion reconciliation are unchanged; they simply operate over
    ///   what was actually read, so an item under a container this pass did not look at is never
    ///   claimed as deleted. `nil` performs the full reconciliation.
    /// - Parameter onDiscovered: Invoked as each depth wave completes, with the creations and updates
    ///   that wave revealed and no earlier one had. Waves run shallowest-first, so successive calls
    ///   are already ordered parents-before-children. Deletions are deliberately absent: an item
    ///   missing from one directory may have moved into another the walk has not reached yet, so
    ///   they are only decidable once it is complete (see `survivingOcIds` below).
    func scanMaterialisedItemsForRemoteChanges(
        restrictedToContainers: Set<NSFileProviderItemIdentifier>? = nil,
        onDiscovered: (@Sendable ([SendableItemMetadata]) -> Void)? = nil
    ) async -> (
        updated: [SendableItemMetadata], deleted: [SendableItemMetadata], hadFailure: Bool
    ) {
        logger.debug("Checking materialised items for changes on the server...")

        defer {
            logger.debug("Completed checking materialised items for changes on the server.")
        }

        // Unlike when enumerating items we can't progressively enumerate items as we need to
        // wait to see which items are truly deleted and which have just been moved elsewhere.
        // Visited folders and downloaded files. Sort in terms of their remote URLs.
        // This way we ensure we visit parent folders before their children.
        // Trashed rows are excluded. Trashing rewrites an item's `serverUrl` to the trashbin but
        // does not set `deleted`, and a folder keeps `visitedDirectory`, so a trashed folder stayed
        // in the materialised set and this scan PROPFINDed it through the ordinary DAV path — which
        // 404s. That 404 is then read as "the item is gone", reporting the item deleted and hard-
        // removing the row the trash reconciliation derives permanent deletions from. Trash has its
        // own enumeration path (``enumerateTrashChanges(for:anchor:)``, via `listingTrashAsync`).
        let materialisedItems = dbManager
            .materialisedItemMetadatas(account: account.ncKitAccount)
            .filter { !$0.deleted && !$0.isTrashed }
            .sorted { $0.remotePath().count < $1.remotePath().count }

        var accumulatedCreations = [SendableItemMetadata]()
        var accumulatedUpdates = [SendableItemMetadata]()
        var accumulatedDeletions = [SendableItemMetadata]()
        var scannedItemIds = Set<String>()
        // Track read failures so one unreadable folder no longer aborts the whole scan (see the
        // read-error branch below). `hadReadFailure` is returned to the caller so it can avoid
        // advancing the working-set sync point past changes this pass could not discover.
        var hadReadFailure = false
        var failedItemIds = Set<String>()
        // What `onDiscovered` has already been handed, so each wave emits only its own delta.
        var emittedUpdateOcIds = Set<String>()

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
        // `materialisedItems` stays the full set below — it is what the descendant checks consult.
        // Only the seed of the walk narrows.
        var scanQueue = materialisedItems
        if let restrictedToContainers {
            let targetOcIds = Set(restrictedToContainers.map(\.rawValue))
            scanQueue = materialisedItems.filter { targetOcIds.contains($0.ocId) }
            logger.info("Scanning \(scanQueue.count) push-targeted container(s) instead of \(materialisedItems.count) materialised item(s).")
        }
        var enqueuedDirectoryIds = Set(scanQueue.filter(\.directory).map(\.ocId))

        /// The reads are issued concurrently, in waves grouped by remote-path depth.
        ///
        /// Depth is what makes concurrency safe here. The only ordering this loop depends on is that
        /// a directory is read before the items it covers: a depth-1 read records its unchanged
        /// direct children in `scannedItemIds`, which is what stops them being read individually.
        /// Those children are always exactly one level deeper, and a changed child directory
        /// enqueued mid-scan is deeper still — so processing strictly shallowest-depth-first
        /// preserves every coverage decision the sequential walk made, while items *within* one
        /// depth can never cover one another and are therefore independent.
        ///
        /// Results are merged back in wave order, single-threaded, so the accumulators and
        /// `scannedItemIds` evolve exactly as before; only the network waiting overlaps. That
        /// waiting was the whole cost: one measured scan spent 76s on 669 sequential PROPFINDs of
        /// ~100ms each to surface zero changes.
        func remotePathDepth(_ metadata: SendableItemMetadata) -> Int {
            metadata.remotePath().reduce(into: 0) { count, character in
                if character == "/" {
                    count += 1
                }
            }
        }

        while !scanQueue.isEmpty {
            guard let waveDepth = scanQueue.map(remotePathDepth).min() else { break }

            let wave = scanQueue.filter { remotePathDepth($0) == waveDepth }
            scanQueue.removeAll { remotePathDepth($0) == waveDepth }

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

            let waveResults = await concurrentlyReadForWorkingSetScan(toRead)

            for (itemToScan, readResult) in waveResults {
                // A shallower item in this same wave cannot have covered this one, but a read
                // enqueued before an earlier merge in this wave can have: re-check.
                guard !scannedItemIds.contains(itemToScan.ocId) else { continue }

                let itemRemoteUrl = itemToScan.remotePath()
                let changes = readResult.changes ?? ChangeSet()

                if readResult.error?.errorCode == 404 {
                    accumulatedDeletions.append(itemToScan)
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
                    continue
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
                                            && ($0.hasSameRemotePath(as: childPath)
                                                || $0.isDescendant(of: childPath))
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

            // Hand this wave over before starting the next. Emitting the delta with a persistent
            // seen-set keeps first-occurrence ordering identical to the single dedup below.
            if let onDiscovered {
                let waveDiscoveries = (accumulatedCreations + accumulatedUpdates).filter {
                    emittedUpdateOcIds.insert($0.ocId).inserted
                }

                if !waveDiscoveries.isEmpty {
                    onDiscovered(waveDiscoveries)
                }
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

        if hadReadFailure {
            logger.error(
                "Working-set remote-change scan was incomplete: \(failedItemIds.count) item(s) could not be read and were skipped; their changes will be retried on the next scan. Not advancing the working-set sync point past the undiscovered changes.",
                [.account: account]
            )
        }

        return (updated: discoveredUpdates, deleted: reportedDeletions, hadFailure: hadReadFailure)
    }

    ///
    /// Read `items` from the server concurrently, at most ``workingSetScanConcurrency`` at a time,
    /// returning each paired with its result **in the original order**.
    ///
    /// Restoring the order matters: the caller merges these results single-threaded, so it observes
    /// exactly the sequence the old sequential walk produced. Only the network waiting overlaps.
    ///
    private func concurrentlyReadForWorkingSetScan(
        _ items: [SendableItemMetadata]
    ) async -> [(SendableItemMetadata, RemoteReadResult)] {
        await withTaskGroup(of: (Int, RemoteReadResult).self) { group in
            var results = [Int: RemoteReadResult]()
            var next = 0

            func addRead(_ index: Int) {
                let item = items[index]
                group.addTask {
                    let result = await Enumerator.readServerUrl(
                        item.remotePath(),
                        account: self.account,
                        remoteInterface: self.remoteInterface,
                        dbManager: self.dbManager,
                        depth: item.directory ? .targetAndDirectChildren : .target,
                        log: self.logger.log
                    )
                    return (index, result)
                }
            }

            while next < min(workingSetScanConcurrency, items.count) {
                addRead(next)
                next += 1
            }

            while let (index, result) = await group.next() {
                results[index] = result

                if next < items.count {
                    addRead(next)
                    next += 1
                }
            }

            return items.indices.compactMap { index in
                results[index].map { (items[index], $0) }
            }
        }
    }
}

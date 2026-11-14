//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

/// How many reads of one kind run at a time, so a sweep over many directories does not open one connection per directory.
let workingSetScanConcurrency = 6

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
    /// re-derived from the database (``FilesDatabaseManager/pendingWorkingSetChanges(since:)``), which
    /// leaves out state the system already received through an item page or an action reply. The
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

                let serverChanges = await scanMaterialisedItemsForRemoteChanges()
                let localChanges = dbManager.pendingWorkingSetChanges(since: date)
                let pendingLocalChanges = localChanges ?? ([], [])
                // Only a failed local read keeps the incoming anchor: the pending local changes are derived from the anchor date, so advancing past a read which failed would lose them. A failed remote read does not hold the anchor back. Remote changes are found by comparing the server with the stored rows, not by date, and a directory whose listing failed is forgotten by the scan, so it is listed again on the next pass either way. Keeping the anchor made the next pass re-derive and re-deliver everything since the old date.
                let hadFailure = serverChanges.storeUnreadable || localChanges == nil

                let changes = ChangeSet(
                    mergingUpdated: [serverChanges.updated, pendingLocalChanges.updated],
                    deleted: [serverChanges.deleted, pendingLocalChanges.deleted]
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

            // Intermediate batches use continuation anchors. If the local changes could not be read,
            // keep the incoming anchor for the final batch so they are derived again on the next signal.
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
    /// Find the remote changes to the working set by walking the directory tree from the account
    /// root, persist them, and return them.
    ///
    /// The server bumps the etag of every ancestor when something changes below it, so one listing of
    /// the root shows which top-level directories changed, and the walk descends only into directories
    /// whose remote state (etag and permissions) differs from the state at which the scan last
    /// reconciled them, and which hold a materialized item. Unchanged subtrees cost nothing; an idle
    /// domain costs one request. Files are never read on their own, their parent's listing carries
    /// their state.
    ///
    /// The reconciled state is remembered per directory in `scannedRemoteState`, as the parent's
    /// listing reports it (the server rewrites a mount point's permissions there), written only after
    /// the whole pass, and only for directories whose descended children all succeeded. A failed
    /// listing forgets the state of the directory and its ancestors, so the next pass descends there
    /// again from the root. Directories on external storages are listed on every pass, with
    /// everything above and below them, because the server notices changes made behind its back only
    /// when the directory is read. See ``ScanDescentDecision``.
    ///
    /// The returned changes are reported to the working-set change observer directly. Relying on
    /// ``FilesDatabaseManager/pendingWorkingSetChanges(since:)`` alone to re-derive the report from
    /// the database loses changes to non-materialised items and to items in subtrees whose own parent
    /// directory did not change, because that reconstruction is gated on the materialised set and
    /// `syncTime`.
    ///
    /// - Returns: The discovered creations and updates (combined into `updated`), the items that
    ///   were marked deleted, whether the materialised items could not be read from the store at all,
    ///   and how many directories could not be listed and are left for the next pass.
    ///
    func scanMaterialisedItemsForRemoteChanges() async -> (
        updated: [SendableItemMetadata], deleted: [SendableItemMetadata], storeUnreadable: Bool, failedListingCount: Int
    ) {
        logger.debug("Checking materialised items for changes on the server...")

        defer {
            logger.debug("Completed checking materialised items for changes on the server.")
        }

        // Trashed rows are not below the account root: trashing moves `serverUrl` to the trashbin.
        guard let somethingIsMaterialised = dbManager.hasMaterialisedItem(below: account.davFilesUrl) else {
            return (updated: [], deleted: [], storeUnreadable: true, failedListingCount: 0)
        }

        guard somethingIsMaterialised else {
            logger.info("Nothing is materialised, so there is nothing to check on the server.")
            return (updated: [], deleted: [], storeUnreadable: false, failedListingCount: 0)
        }

        var state = WorkingSetScanState(scannedRemoteStates: dbManager.scannedRemoteStates() ?? [:])

        let rootMetadata = dbManager.itemMetadata(ocId: NSFileProviderItemIdentifier.rootContainer.rawValue)
        _ = await listDirectory(
            at: account.davFilesUrl,
            ocId: NSFileProviderItemIdentifier.rootContainer.rawValue,
            metadata: rootMetadata,
            remoteStateAsListed: nil,
            ancestorOcIds: [],
            state: &state
        )

        var deletions = expandMissingDirectories(state: &state)
        dbManager.recordScannedRemoteStates(state.reconciled, forgetting: state.forgotten)

        // Deduplicate the discovered new/updated metadata by ocId, preserving order; the root row is the system's, not ours to report.
        var seenUpdatedOcIds = Set<String>()
        let firstCopies = state.createdAndUpdated.filter {
            $0.ocId != NSFileProviderItemIdentifier.rootContainer.rawValue && seenUpdatedOcIds.insert($0.ocId).inserted
        }

        // A row which a later listing of the same pass moved again, or deleted, is reported as it is stored now.
        var storeUnreadable = false
        let storedRows = dbManager.itemMetadatas(ocIds: firstCopies.map(\.ocId))
        var discoveredUpdates = [SendableItemMetadata]()

        for copy in firstCopies {
            guard let storedRows else {
                discoveredUpdates.append(copy)
                continue
            }

            // A row gone since its listing was removed meanwhile or never written, so it is not reported and the pass counts as incomplete.
            guard let stored = storedRows[copy.ocId] else {
                storeUnreadable = true
                continue
            }

            if stored.deleted {
                deletions.append(stored)
            } else {
                discoveredUpdates.append(stored)
            }
        }

        if discoveredUpdates.isEmpty, deletions.isEmpty {
            logger.info("No remote changes found in materialised items.")
        }

        logger.info(
            "Working-set scan finished.",
            [.name: "\(state.listedCount) listed, \(state.skippedCount) skipped, \(state.failedCount) failed"]
        )

        return (updated: discoveredUpdates, deleted: deletions, storeUnreadable: storeUnreadable, failedListingCount: state.failedCount)
    }

    ///
    /// List one directory, descend into the changed directories below it which hold something
    /// materialized, and remember the directory's state when everything below it succeeded.
    ///
    /// - Parameters:
    ///   - remoteStateAsListed: The directory's state as its parent's listing reported it, the state
    ///     the next pass compares against; `nil` for the root, which is listed on every pass.
    /// - Returns: Whether this directory and every directory it descended into were listed.
    ///
    private func listDirectory(
        at remotePath: String,
        ocId: String,
        metadata: SendableItemMetadata?,
        remoteStateAsListed: String?,
        ancestorOcIds: [String],
        state: inout WorkingSetScanState
    ) async -> Bool {
        logger.debug("Listing a directory for working-set changes.", [.url: remotePath])
        state.listedCount += 1

        let readResult = await Enumerator.readServerUrl(
            remotePath,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            depth: .targetAndDirectChildren,
            recordVisit: false,
            log: logger.log
        )

        let isRoot = ocId == NSFileProviderItemIdentifier.rootContainer.rawValue

        if readResult.error?.errorCode == 404, !isRoot {
            // The directory is gone; its subtree is expanded once the pass is over and moves are known.
            logger.debug("Directory is gone from the server; its subtree is reported deleted.", [.url: remotePath])
            if var gone = metadata {
                gone.deleted = true
                gone.syncTime = Date()
                state.missing.append(gone)
                state.notFoundOcIds.insert(ocId)
            }
            return true
        }

        if let readError = readResult.error, readError != .success {
            // The rest of the working set is still scanned. This directory and its ancestors forget
            // their reconciled state, so the next pass descends here again whatever else changed.
            // See nextcloud/desktop#10442.
            logger.error(
                "Directory listing failed; the directory and its ancestors are scanned again on the next pass.",
                [.error: readError, .url: remotePath]
            )
            state.failedCount += 1
            state.forgotten.formUnion(ancestorOcIds)
            state.forgotten.insert(ocId)
            return false
        }

        let changes = readResult.changes ?? ChangeSet()
        state.createdAndUpdated.append(contentsOf: changes.created + changes.updated)
        state.survivingOcIds.formUnion(changes.created.map(\.ocId))
        state.survivingOcIds.formUnion(changes.updated.map(\.ocId))
        state.missing.append(contentsOf: changes.deleted)

        // The listing carries the server's state; the walk decides by the stored one, where a visit,
        // a pin and a download are recorded, and reports the rows as the store holds them after the write.
        guard let rows = readResult.metadatas,
              case let listedRows = Enumerator.storedRows(for: rows, dbManager: dbManager),
              let listedTarget = listedRows.first
        else {
            return true
        }

        // An item any successful listing returned exists on the server, whatever a read of it at an
        // older path answers later in the pass.
        state.survivingOcIds.formUnion(listedRows.map(\.ocId))

        var allDescentsSucceeded = true

        for child in listedRows.dropFirst() where child.directory {
            guard !isLockFileName(child.fileName), state.listedOcIds.insert(child.ocId).inserted else {
                continue
            }

            let childPath = child.remotePath()
            // A failed read descends: better one listing too many than a subtree never checked.
            let decision = ScanDescentDecision(
                child: child,
                holdsMaterialisedItem: dbManager.hasMaterialisedItem(below: childPath) ?? true,
                holdsExternalStorage: dbManager.hasExternalStorage(below: childPath) ?? true,
                reconciledState: state.scannedRemoteStates[child.ocId]
            )

            switch decision {
                case .nothingTracked:
                    continue
                case .unchanged:
                    logger.debug("Skipping a directory whose subtree is unchanged since the last scan.", [.url: childPath, .eTag: child.etag])
                    state.skippedCount += 1
                    continue
                case .descend:
                    break
            }

            let descended = await listDirectory(
                at: childPath,
                ocId: child.ocId,
                metadata: child,
                remoteStateAsListed: FilesDatabaseManager.scanRemoteState(of: child),
                ancestorOcIds: ancestorOcIds + [ocId],
                state: &state
            )
            allDescentsSucceeded = allDescentsSucceeded && descended
        }

        if allDescentsSucceeded, let remoteStateAsListed {
            state.reconciled[ocId] = remoteStateAsListed
        }

        return allDescentsSucceeded
    }

    ///
    /// Turn the tombstones a pass collected into the deletions to report, expanding deleted directories
    /// to everything below them now that moves are known.
    ///
    /// A directory which reappeared anywhere in the pass was moved, not deleted, and is dropped. A
    /// deleted directory whose name is taken by a live directory with another identifier is reported
    /// alone: the new directory's own listing reconciles the children.
    ///
    private func expandMissingDirectories(state: inout WorkingSetScanState) -> [SendableItemMetadata] {
        var deletions: [SendableItemMetadata] = []
        var reportedOcIds = Set<String>()
        let now = Date()

        for missing in state.missing where !state.survivingOcIds.contains(missing.ocId) {
            guard reportedOcIds.insert(missing.ocId).inserted else {
                continue
            }

            // The listing concluded about the path it read. A row which moved away from that path
            // since, or which the user has started uploading to, is not what the listing concluded about.
            let current = dbManager.itemMetadata(ocId: missing.ocId)

            // A 404 is about the path which was read, so any move away from it makes the answer stale.
            // A listing's own conclusion is about the children the server returned, which an ancestor
            // renamed meanwhile does not put in doubt; a rename of the row itself does, because the
            // listing did not look for that name.
            let movedSince = state.notFoundOcIds.contains(missing.ocId)
                ? current.map { !$0.hasSameRemotePath(as: missing.remotePath()) }
                : current.map { $0.fileName != missing.fileName }

            if movedSince == true, let current {
                logger.debug("Ignoring the deletion of an item which has since moved.", [.item: missing.ocId, .url: missing.remotePath()])
                revive(current)
                continue
            }

            if (current ?? missing).status >= Status.inUpload.rawValue {
                logger.info("Skipping deletion of item with pending upload.", [.item: missing.ocId])
                current.map(revive)
                continue
            }

            var tombstone = current ?? missing
            tombstone.deleted = true

            guard missing.directory else {
                deletions.append(tombstone)
                continue
            }

            // A folder parked at a holding path kept its children for the placement which is to come.
            if (current ?? missing).isAtHoldingPath {
                logger.debug("A folder waiting at a holding path is reported alone.", [.item: missing.ocId])
                deletions.append(tombstone)
                continue
            }

            let recreated = dbManager.liveItemOccupies(serverUrl: missing.serverUrl, fileName: missing.fileName, otherThan: missing.ocId) ?? false
            if recreated {
                logger.debug("A deleted directory was recreated under its name; only the old directory is reported.", [.item: missing.ocId, .url: missing.remotePath()])
                if dbManager.itemMetadata(ocId: missing.ocId)?.deleted != true {
                    dbManager.deleteItemMetadata(ocId: missing.ocId)
                }
                deletions.append(tombstone)
                continue
            }

            guard let subtree = dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: missing.ocId, stampingSyncTime: now) else {
                deletions.append(tombstone)
                continue
            }

            for var row in subtree {
                let isTheDirectory = row.ocId == missing.ocId
                guard isTheDirectory || (!state.survivingOcIds.contains(row.ocId) && reportedOcIds.insert(row.ocId).inserted) else {
                    continue
                }
                row.deleted = true
                row.syncTime = now
                deletions.append(row)
            }
        }

        return deletions
    }

    ///
    /// Put a row back which a listing tombstoned although the pass decided not to report its deletion.
    ///
    /// The listing writes its conclusion as it goes, while the pass decides about the deletion once every
    /// listing is in. A row left tombstoned would reach the system as a deletion through the local
    /// derivation, which reports what the store holds, whatever this pass reports.
    ///
    private func revive(_ row: SendableItemMetadata) {
        guard row.deleted else {
            return
        }

        var alive = row
        alive.deleted = false
        alive.syncTime = Date()
        dbManager.addItemMetadata(alive)
    }
}

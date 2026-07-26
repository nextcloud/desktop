//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

extension Enumerator {
    ///
    /// Pop the next batch of trash deletions from ``changeBuffer`` and report it.
    ///
    /// `NKTrash` items have no ETag; the only trash change of interest is a *permanent* remote deletion,
    /// detected as a local trash row absent from the remote listing. Those orphans are derived once (see
    /// ``enumerateTrashChanges(for:anchor:)``) and drained here one capped batch per invocation so a large
    /// permanent-purge cannot exceed the framework's per-batch limit. Trash preserves its incoming anchor
    /// (like a regular container), so the same anchor is returned on every batch. Each delivered orphan is
    /// soft-deleted (`deleteItemMetadata`) only after its batch finishes, so an interrupted drain keeps its
    /// row — the reconciliation re-derives it (the row still carries a trash `serverUrl`) rather than
    /// dropping it.
    ///
    private func drainTrashDeletions(
        for observer: NSFileProviderChangeObserver, anchor: NSFileProviderSyncAnchor, suggested: Int?
    ) {
        let batch = changeBuffer.takeBatch(maxItems: effectiveBatchSize(suggested: suggested))
        let orphanedIdentifiers = batch.deleted.map { NSFileProviderItemIdentifier($0.ocId) }

        if orphanedIdentifiers.isEmpty == false {
            observer.didDeleteItems(withIdentifiers: orphanedIdentifiers)
        }

        // Soft-delete delivered orphans before finishing the batch: they are already reported
        // (didDeleteItems above), and writing the DB before finishEnumeratingChanges keeps the local
        // state consistent with what the observer has been told by the time the batch is acknowledged.
        for metadata in batch.deleted {
            dbManager.deleteItemMetadata(ocId: metadata.ocId)
        }

        observer.finishEnumeratingChanges(upTo: anchor, moreComing: batch.moreComing)

        logger.debug("Reported trash deletion batch. deleted: \(batch.deleted.count), moreComing: \(batch.moreComing)")
    }

    ///
    /// Full enumeration of the trash container.
    ///
    /// The trash only ever lists items deleted on this device, which cannot exist before the initial
    /// content enumeration of a domain. The initial trash enumeration therefore finishes with an
    /// empty set once trash support has been confirmed in the server capabilities.
    ///
    func enumerateTrashItems(for observer: NSFileProviderEnumerationObserver) {
        logger.info("Enumerating items in trash.", [.account: account.ncKitAccount, .url: serverUrl])

        Task { [weak self] in
            guard let self else {
                return
            }

            let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(account: account)

            guard let capabilities, error == .success else {
                logger.error("Could not acquire capabilities, cannot check trash.", [.error: error])
                observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                return
            }

            guard capabilities.files?.undelete == true else {
                logger.error("Trash is unsupported on server, cannot enumerate items.")
                observer.finishEnumeratingWithError(NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError))
                return
            }

            // We only want to list items deleted on the local device.
            // That cannot happen before the initial content enumeration for a file provider domain because the latter does not exist yet.
            // Hence the initial trash content enumeration can be finished with an empty set.
            observer.finishEnumerating(upTo: nil)
        }
    }

    ///
    /// Change enumeration of the trash container: list the remote trash, reconcile it against the local
    /// trash to find permanently-deleted (orphaned) items, buffer them, and drain them one capped batch
    /// at a time via ``drainTrashDeletions(for:anchor:suggested:)``.
    ///
    func enumerateTrashChanges(for observer: NSFileProviderChangeObserver, anchor: NSFileProviderSyncAnchor) {
        logger.debug("Enumerating changes in trash.", [.account: account.ncKitAccount])

        let anchorKey = String(data: anchor.rawValue, encoding: .utf8) ?? ""

        Task { [weak self] in
            guard let self else {
                return
            }

            // Continuation of an in-progress drain: serve the next buffered batch without re-listing the
            // remote trash (re-deriving after per-batch soft-deletes would re-report the wrong subset).
            if changeBuffer.isPrimed(forKey: anchorKey) {
                logger.debug("Trash change buffer primed for anchor \(anchorKey); draining next batch without re-listing.", [.account: account.ncKitAccount])
                drainTrashDeletions(for: observer, anchor: anchor, suggested: observer.suggestedBatchSize)
                return
            }
            changeBuffer.reset()

            let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(account: account)

            guard let capabilities, error == .success else {
                logger.error("Could not acquire capabilities, cannot check trash.", [.error: error])
                observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                return
            }

            guard capabilities.files?.undelete == true else {
                logger.error("Trash is unsupported on server. Cannot enumerate changes.")

                observer.finishEnumeratingWithError(
                    NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError)
                )
                return
            }

            let domain = domain
            let enumeratedItemIdentifier = enumeratedItemIdentifier

            let (_, trashedItems, _, trashReadError) = await remoteInterface.listingTrashAsync(
                filename: nil,
                showHiddenFiles: true,
                account: account.ncKitAccount,
                options: .init(),
                taskHandler: { task in
                    if let domain {
                        NSFileProviderManager(for: domain)?.register(
                            task,
                            forItemWithIdentifier: enumeratedItemIdentifier,
                            completionHandler: { _ in }
                        )
                    }
                }
            )

            guard trashReadError == .success else {
                let error = trashReadError.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier) ?? NSFileProviderError(.cannotSynchronize)
                observer.finishEnumeratingWithError(error)
                return
            }

            // Orphans = local trash rows absent from the remote trash listing = permanently deleted
            // remotely. Derive once, then drain in batches.
            let remoteSet = Set((trashedItems ?? []).map(\.ocId))
            let orphans = dbManager.trashedItemMetadatas(account: account)
                .filter { !remoteSet.contains($0.ocId) }

            for orphan in orphans {
                logger.info("Permanently deleting remote trash item which could not be matched with a local one.", [.item: orphan.ocId])
            }

            changeBuffer.prime(key: anchorKey, updated: [], deleted: orphans)
            drainTrashDeletions(for: observer, anchor: anchor, suggested: observer.suggestedBatchSize)
        }
    }
}

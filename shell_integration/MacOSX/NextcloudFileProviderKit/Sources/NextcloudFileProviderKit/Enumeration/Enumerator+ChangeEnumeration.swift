//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

///
/// Remote change derivation for a regular directory container
/// (`NSFileProviderEnumerator.enumerateChanges`). The working set and trash are handled in
/// ``Enumerator`` (WorkingSetScan) and (Trash) respectively.
///
extension Enumerator {
    ///
    /// Read the container from the server, diff it against the local database and report the
    /// resulting creations, updates and deletions to the change observer.
    ///
    /// A 404 from the folder listing reports a deletion once the database records it.
    /// A `nil` change set means the database write that derives the changes failed.
    ///
    func enumerateContainerChanges(
        for observer: NSFileProviderChangeObserver,
        anchor: NSFileProviderSyncAnchor
    ) {
        logger.info("Enumerating changes in item.", [.url: serverUrl])

        let anchorKey = String(data: anchor.rawValue, encoding: .utf8) ?? ""

        // No matter what happens here we finish enumeration in some way, either from the error
        // handling below or from draining the change buffer.
        Task { [weak self] in
            guard let self else {
                return
            }

            // Continuation of an in-progress drain: serve the next buffered batch without re-reading
            // the container (the depth-1 read is destructive, so re-deriving would lose the remainder).
            let isPrimed = readDeliveryState(reportingTo: observer) {
                try changeBuffer.isPrimed(forKey: anchorKey)
            }

            guard let isPrimed else {
                return
            }

            if isPrimed {
                logger.debug("Container change buffer primed for anchor \(anchorKey); draining next batch without re-reading.", [.url: serverUrl])
                drainChangeBuffer(
                    for: observer,
                    finalAnchor: anchor,
                    suggested: observer.suggestedBatchSize
                )
                return
            }
            changeBuffer.reset()

            let readResult = await Self.readServerUrl(
                serverUrl,
                account: account,
                sourceMetadata: enumeratedItemMetadata,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                log: logger.log
            )
            let readError = readResult.error

            guard dbManager.isForCurrentDAVAccount() else {
                observer.finishEnumeratingWithError(NSFileProviderError(.syncAnchorExpired))
                return
            }

            if let readError {
                logger.error("Finished enumerating changes.", [.url: serverUrl, .error: readError])

                let error = readError.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: enumeratedItemIdentifier) ?? NSFileProviderError(.cannotSynchronize)

                if readError.isNotFoundError {
                    guard let itemMetadata = enumeratedItemMetadata else {
                        logger.error("Missing folder metadata. Could not report deletion.")
                        observer.finishEnumeratingWithError(error)
                        return
                    }

                    let recordedDeletion = if itemMetadata.directory {
                        dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: itemMetadata.ocId) != nil
                    } else {
                        dbManager.deleteItemMetadata(ocId: itemMetadata.ocId)
                    }
                    guard recordedDeletion else {
                        logger.error("Could not record the deletion. Asking File Provider to retry.", [.item: itemMetadata.ocId])
                        observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                        return
                    }

                    // A single deletion — small enough to report directly without buffering.
                    completeChangesBatch(
                        observer,
                        updated: [],
                        deleted: [itemMetadata],
                        anchor: anchor,
                        moreComing: false,
                        acknowledgement: nil,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager
                    )
                    return
                } else if readError.isNoChangesError { // All is well, just no changed etags
                    logger.info("Error was to say no changed files - not bad error. Finishing change enumeration.")
                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
                    return
                }

                observer.finishEnumeratingWithError(error)
                return
            }

            guard let changes = readResult.changes else {
                // The database write that derives the change set failed; report an error to the
                // framework, mirroring the previous nil-guard behavior of completeChangesObserver.
                let error = NSError.fileProviderErrorForNonExistentItem(withIdentifier: enumeratedItemIdentifier)
                logger.error("Received no change set for container. Finishing enumeration of changes with error.", [.error: error])
                observer.finishEnumeratingWithError(error)
                return
            }

            logger.info("Finished reading remote changes.", [.account: account.ncKitAccount, .url: serverUrl])

            // Sort created+updated parents-before-children (see completeChangesBatch) and buffer the
            // full set so it can be delivered one batch at a time across the framework's moreComing
            // re-invocations without exceeding the per-batch limit.
            let sortedUpdated = changes.createdAndUpdated
                .sorted { $0.remotePath().count < $1.remotePath().count }
            changeBuffer.prime(
                key: anchorKey,
                finalAnchorRawValue: anchor.rawValue,
                updated: sortedUpdated,
                deleted: changes.deleted
            )

            drainChangeBuffer(
                for: observer,
                finalAnchor: anchor,
                suggested: observer.suggestedBatchSize
            )
        }
    }
}

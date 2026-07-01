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
    /// A 404 is treated as a deletion of the container itself (reported, not an error). A
    /// "no changed etags" reply finishes cleanly with no changes. A `nil` change set means the
    /// database write that derives the changes failed and is reported as an error.
    ///
    func enumerateContainerChanges(
        for observer: NSFileProviderChangeObserver, anchor: NSFileProviderSyncAnchor
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
            // A regular container preserves its incoming anchor, so startAnchor and finalAnchor match.
            if changeBuffer.isPrimed(forKey: anchorKey) {
                logger.debug("Container change buffer primed for anchor \(anchorKey); draining next batch without re-reading.", [.url: serverUrl])
                drainChangeBuffer(
                    for: observer,
                    startAnchor: anchor,
                    finalAnchor: anchor,
                    suggested: observer.suggestedBatchSize
                )
                return
            }
            changeBuffer.reset()

            let readResult = await Self.readServerUrl(
                serverUrl,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                log: logger.log
            )
            let readError = readResult.error

            guard readError == nil else {
                logger.error("Finished enumerating changes.", [.url: serverUrl, .error: readError])

                let error = readError?.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: enumeratedItemIdentifier) ?? NSFileProviderError(.cannotSynchronize)

                if readError!.isNotFoundError {
                    logger.info("404 error means item no longer exists. Deleting metadata and reporting deletion without error.", [.url: serverUrl])

                    guard let itemMetadata = enumeratedItemMetadata else {
                        logger.error("Invalid enumeratedItemMetadata. Could not delete metadata nor report deletion.")
                        observer.finishEnumeratingWithError(error)
                        return
                    }

                    if itemMetadata.directory {
                        if dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: itemMetadata.ocId) == nil {
                            logger.error("Something went wrong when recursively deleting directory. It's metadata was not found. Cannot report it as deleted.")
                        }
                    } else {
                        dbManager.deleteItemMetadata(ocId: itemMetadata.ocId)
                    }

                    // A single deletion — small enough to report directly without buffering.
                    completeChangesBatch(
                        observer,
                        updated: [],
                        deleted: [itemMetadata],
                        anchor: anchor,
                        moreComing: false,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager
                    )
                    return
                } else if readError!.isNoChangesError { // All is well, just no changed etags
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
            changeBuffer.prime(key: anchorKey, updated: sortedUpdated, deleted: changes.deleted)

            drainChangeBuffer(
                for: observer,
                startAnchor: anchor,
                finalAnchor: anchor,
                suggested: observer.suggestedBatchSize
            )
        }
    }
}

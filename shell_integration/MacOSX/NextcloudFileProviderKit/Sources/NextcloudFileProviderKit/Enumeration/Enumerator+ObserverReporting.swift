//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

///
/// Translating derived metadata into the `NSFileProviderEnumerationObserver` /
/// `NSFileProviderChangeObserver` callbacks the framework expects, plus the shared recovery used
/// when an item's parent identifier is not yet known locally.
///
extension Enumerator {
    ///
    /// Report one page of items to the enumeration observer and finish on the given continuation page.
    ///
    /// Callers paginate: each invocation passes a single page's worth of `itemMetadatas` together with the
    /// already-encoded `nextPage` to continue from (`nil` ends the enumeration). Keeping each page under
    /// the framework's per-page limit is the caller's responsibility — see ``effectiveBatchSize(suggested:)``.
    ///
    func completeEnumerationObserver(
        _ observer: NSFileProviderEnumerationObserver,
        nextPage: NSFileProviderPage?,
        itemMetadatas: [SendableItemMetadata],
        handleInvalidParent: Bool = true
    ) {
        Task {
            do {
                let items = try await itemMetadatas.toFileProviderItems(
                    account: account, remoteInterface: remoteInterface, dbManager: dbManager, log: self.logger.log
                )

                Task { @MainActor in
                    observer.didEnumerate(items)
                    logger.info("Did enumerate \(items.count) items. Next page is nil: \(nextPage == nil)")
                    observer.finishEnumerating(upTo: nextPage)
                }
            } catch let error as NSError { // This error can only mean a missing parent item identifier
                guard handleInvalidParent else {
                    logger.info("Not handling invalid parent in enumeration.")
                    observer.finishEnumeratingWithError(error)
                    return
                }

                do {
                    let metadata = try await attemptInvalidParentRecovery(
                        error: error,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager
                    )

                    completeEnumerationObserver(
                        observer,
                        nextPage: nextPage,
                        itemMetadatas: [metadata] + itemMetadatas,
                        handleInvalidParent: false
                    )
                } catch {
                    observer.finishEnumeratingWithError(error)
                }
            }
        }
    }

    ///
    /// Pop the next batch from ``changeBuffer`` and report it.
    ///
    /// Intermediate batches (`moreComing == true`) return `startAnchor` — the anchor the enumeration was
    /// invoked with — so that if the drain is interrupted the framework resumes *behind* every
    /// undelivered item rather than advancing the sync point past changes still sitting in the buffer.
    /// Only the final batch returns `finalAnchor` (the working set advances to ``currentAnchor``; a
    /// regular container preserves its incoming anchor).
    ///
    func drainChangeBuffer(
        for observer: NSFileProviderChangeObserver,
        startAnchor: NSFileProviderSyncAnchor,
        finalAnchor: NSFileProviderSyncAnchor,
        suggested: Int?
    ) {
        let batch = changeBuffer.takeBatch(maxItems: effectiveBatchSize(suggested: suggested))
        logger.info(
            "Reporting change batch. updated: \(batch.updated.count), deleted: \(batch.deleted.count), moreComing: \(batch.moreComing)",
            [.item: enumeratedItemIdentifier]
        )
        completeChangesBatch(
            observer,
            updated: batch.updated,
            deleted: batch.deleted,
            anchor: batch.moreComing ? startAnchor : finalAnchor,
            moreComing: batch.moreComing,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )
    }

    ///
    /// Report one already-sized batch of changes to the change observer and finish the batch.
    ///
    /// `updated` and `deleted` are a single batch's worth of metadata (kept under the framework's per-batch
    /// limit by the caller via ``effectiveBatchSize(suggested:)``). The file-provider framework does not
    /// distinguish created from updated items, so both arrive through `updated`; it must already be sorted
    /// parents-before-children. A deleted item's database row is removed only once *its* batch has been
    /// delivered, so a batch still waiting in the ``changeBuffer`` keeps its rows for re-derivation should
    /// the drain be interrupted. The missing-parent recovery mirrors ``completeEnumerationObserver`` and is
    /// scoped to this batch.
    ///
    func completeChangesBatch(
        _ observer: NSFileProviderChangeObserver,
        updated: [SendableItemMetadata],
        deleted: [SendableItemMetadata],
        anchor: NSFileProviderSyncAnchor,
        moreComing: Bool,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager
    ) {
        let deletedFileProviderItemIdentifiers = deleted.map { NSFileProviderItemIdentifier($0.ocId) }

        // Per-item trace so a debug archive can reconstruct exactly which items each batch carried.
        for metadata in updated {
            logger.debug("Reporting updated item in change batch.", [.item: metadata.ocId, .name: metadata.fileName])
        }

        for metadata in deleted {
            logger.debug("Reporting deleted item in change batch.", [.item: metadata.ocId, .name: metadata.fileName])
        }

        // Report deletions once, here — not inside the update path, so the invalid-parent retry below
        // does not re-issue them.
        if deletedFileProviderItemIdentifiers.isEmpty == false {
            observer.didDeleteItems(withIdentifiers: deletedFileProviderItemIdentifiers)
        }

        reportBatchUpdates(
            observer,
            updated: updated,
            deletedToRemove: deleted,
            anchor: anchor,
            moreComing: moreComing,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )
    }

    ///
    /// Convert this batch's `updated` metadata to items, report them, finish the batch, and hard-remove
    /// the delivered deletions from the database. On a missing-parent error it recovers the parent and
    /// retries the conversion once — deletions are already reported by ``completeChangesBatch`` and are
    /// not re-issued here.
    ///
    private func reportBatchUpdates(
        _ observer: NSFileProviderChangeObserver,
        updated: [SendableItemMetadata],
        deletedToRemove: [SendableItemMetadata],
        anchor: NSFileProviderSyncAnchor,
        moreComing: Bool,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        handleInvalidParent: Bool = true
    ) {
        Task { [updated, deletedToRemove] in
            do {
                let updatedItems = try await updated.toFileProviderItems(account: account, remoteInterface: remoteInterface, dbManager: dbManager, log: self.logger.log)

                Task { @MainActor in
                    if !updatedItems.isEmpty {
                        observer.didUpdate(updatedItems)
                    }

                    // Hard-remove delivered deletions before finishing the batch: the items are already
                    // reported (didDeleteItems ran in completeChangesBatch), and doing the DB write before
                    // finishEnumeratingChanges keeps the database consistent with what the observer has
                    // been told by the time the batch is acknowledged.
                    for metadata in deletedToRemove {
                        dbManager.removeItemMetadata(ocId: metadata.ocId)
                    }

                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: moreComing)
                }
            } catch let error as NSError { // This error can only mean a missing parent item identifier
                guard handleInvalidParent else {
                    logger.error("Not handling invalid parent in change enumeration!")
                    observer.finishEnumeratingWithError(error)
                    return
                }

                logger.info("Attempting handling invalid parent in change enumeration.")

                do {
                    let metadata = try await attemptInvalidParentRecovery(
                        error: error,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager
                    )

                    // Prepend the recovered parent so it keeps the parent-before-child ordering within
                    // this batch, then retry once with recovery disabled.
                    reportBatchUpdates(
                        observer,
                        updated: [metadata] + updated,
                        deletedToRemove: deletedToRemove,
                        anchor: anchor,
                        moreComing: moreComing,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager,
                        handleInvalidParent: false
                    )
                } catch {
                    observer.finishEnumeratingWithError(error)
                }
            }
        }
    }

    func attemptInvalidParentRecovery(
        error: NSError,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager
    ) async throws -> SendableItemMetadata {
        logger.info("Attempting recovery from invalid parent identifier.")
        // Try to recover from errors involving missing metadata for a parent
        let userInfoKey =
            FilesDatabaseManager.ErrorUserInfoKey.missingParentServerUrlAndFileName.rawValue
        guard let urlToEnumerate = (error as NSError).userInfo[userInfoKey] as? String else {
            logger.fault("No missing parent server url and filename in error user info.")
            assertionFailure()
            throw NSError()
        }

        logger.info("Recovering from invalid parent identifier at \(urlToEnumerate)")

        let readResult = await Enumerator.readServerUrl(
            urlToEnumerate,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            depth: .target,
            log: logger.log
        )
        guard readResult.error == nil || readResult.error == .success,
              let metadata = readResult.metadatas?.first
        else {
            logger.error(
                """
                Problem retrieving parent for metadata.
                    Error: \(readResult.error?.errorDescription ?? "NONE")
                    Metadatas: \(readResult.metadatas?.count ?? -1)
                """
            )

            throw readResult.error?.fileProviderError ?? NSFileProviderError(.cannotSynchronize)
        }
        // Provide it to the caller method so it can ingest it into the database and fix future errs
        return metadata
    }
}

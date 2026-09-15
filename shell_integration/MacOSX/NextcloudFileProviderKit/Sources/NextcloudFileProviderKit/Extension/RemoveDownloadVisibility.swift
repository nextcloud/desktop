//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// Batches ancestor-container refresh nudges over a short window, so a bulk materialisation nudges
/// each shared ancestor once per window rather than once per file.
///
private final class AncestorRefreshCoalescer: @unchecked Sendable {
    static let shared = AncestorRefreshCoalescer()

    ///
    /// How long to accumulate before nudging, short enough to stay imperceptible in the context
    /// menu and long enough to collapse a folder's sibling files into one pass.
    ///
    private static let windowNanoseconds: UInt64 = 400_000_000

    private let lock = NSLock()
    private var pendingOcIds = Set<String>()
    private var drainScheduled = false

    ///
    /// Take the accumulated batch and reopen the window, so downloads finishing while the batch is
    /// being nudged start a fresh one rather than being dropped.
    ///
    private func claimBatch() -> Set<String> {
        lock.lock()
        defer { lock.unlock() }

        let claimed = pendingOcIds
        pendingOcIds.removeAll()
        drainScheduled = false

        return claimed
    }

    func enqueue(
        ocIds: Set<String>,
        manager: NSFileProviderManager,
        dbManager: FilesDatabaseManager,
        logger: FileProviderLogger
    ) {
        lock.lock()
        pendingOcIds.formUnion(ocIds)

        guard !drainScheduled else {
            lock.unlock()
            return
        }

        drainScheduled = true
        lock.unlock()

        Task {
            try? await Task.sleep(nanoseconds: Self.windowNanoseconds)

            let ocIds = claimBatch()
            let ancestors = dbManager.ancestorContainerIdentifiers(ofFileItemsWithOcIds: ocIds)

            guard !ancestors.isEmpty else { return }

            logger.debug("Refreshing \(ancestors.count) ancestor container(s) for \(ocIds.count) item(s) to update Remove download visibility.")

            for ancestor in ancestors {
                do {
                    try await manager.requestModification(of: [.lastUsedDate], forItemWithIdentifier: ancestor)
                } catch {
                    logger.error("Could not nudge ancestor container to refresh Remove download visibility.", [.item: ancestor, .error: error.localizedDescription])
                }
            }
        }
    }
}

///
/// Refresh the framework's cached snapshot of every ancestor container of the
/// given files — up to and including the root container — so their "Remove
/// download" (`displayEvict`) visibility updates on the whole path (#10085).
///
/// A container's `displayEvict` depends on whether it holds a materialized
/// descendant file, which lives outside the container's own etag, so the
/// framework must be nudged with `requestModification(of: [.lastUsedDate], …)`
/// to re-pull the item; the container's `metadataVersion` folds in the same
/// descendant state (see ``Item/itemVersion``) so the re-pull is not
/// deduplicated away. This is the same nudge ``Item/signalKeepDownloaded`` and
/// the extension-version cache refresh use. Fire and forget.
///
/// It must be invoked from **every** site where a file's `downloaded` flag
/// flips, because they use different code paths:
/// - ``Item/fetchContents(domain:progress:dbManager:)`` (a download) writes
///   `downloaded = true` to the database *before* the system re-enumerates its
///   materialized set, so the ``MaterializedEnumerationObserver`` reconciliation
///   sees no discrepancy and would not otherwise fire here.
/// - The observer covers eviction (a file going dataless) and out-of-band
///   materialization it discovers itself.
///
/// Calls are coalesced over a short window — see ``AncestorRefreshCoalescer`` — so the ancestor
/// walk happens on the drain and latency-sensitive callers are never blocked.
///
func refreshRemoveDownloadVisibility(
    forAncestorsOfFileOcIds ocIds: Set<String>,
    manager: NSFileProviderManager,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    guard !ocIds.isEmpty else { return }

    AncestorRefreshCoalescer.shared.enqueue(ocIds: ocIds, manager: manager, dbManager: dbManager, logger: logger)
}

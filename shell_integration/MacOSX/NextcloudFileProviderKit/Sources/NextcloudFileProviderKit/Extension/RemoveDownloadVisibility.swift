//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// Batches the ocIds of files whose materialisation state changed, then once per window derives
/// their ancestor containers and nudges the ones whose "Remove download" answer actually changed.
///
/// ``Item/itemVersion`` folds a container's evictable-descendant boolean into `metadataVersion`, so
/// the framework discards a re-pull whose version is unchanged and a nudge for a boolean that did
/// not move costs a round trip and a descendant query for nothing.
///
/// Drains are serialised, so a container is never nudged twice concurrently.
///
final class AncestorRefreshCoalescer: @unchecked Sendable {
    /// The instance every production call site accumulates into; tests construct their own.
    static let shared = AncestorRefreshCoalescer()

    /// Long enough to collapse a folder's sibling files into one pass, short enough to stay
    /// imperceptible in the context menu.
    static let defaultWindowNanoseconds: UInt64 = 400_000_000

    private let windowNanoseconds: UInt64
    private let lock = NSLock()
    private var pendingOcIds = Set<String>()

    /// The evictable-descendant boolean each container held when it was last nudged successfully,
    /// absent where that is unknown and the container is therefore always nudged.
    private var lastNudgedEvictableState = [NSFileProviderItemIdentifier: Bool]()

    private var drainTask: Task<Void, Never>?
    private var drainGeneration: UInt64 = 0

    init(windowNanoseconds: UInt64 = AncestorRefreshCoalescer.defaultWindowNanoseconds) {
        self.windowNanoseconds = windowNanoseconds
    }

    ///
    /// Accumulate the given ocIds and, unless a drain is already running, start one with the
    /// closures given here.
    ///
    /// - Parameter evictableState: `nil` where the container's boolean cannot be determined, which
    ///   nudges it rather than skipping it.
    ///
    func enqueue(
        ocIds: Set<String>,
        ancestors: @escaping @Sendable (Set<String>) -> Set<NSFileProviderItemIdentifier>,
        evictableState: @escaping @Sendable (NSFileProviderItemIdentifier) -> Bool?,
        nudge: @escaping @Sendable (NSFileProviderItemIdentifier) async throws -> Void,
        logger: FileProviderLogger
    ) {
        lock.lock()
        defer { lock.unlock() }

        pendingOcIds.formUnion(ocIds)

        guard drainTask == nil else { return }

        drainGeneration &+= 1
        let generation = drainGeneration
        drainTask = Task {
            await self.drain(generation: generation, ancestors: ancestors, evictableState: evictableState, nudge: nudge, logger: logger)
        }
    }

    /// Convenience overload reading from the database and nudging through the framework.
    func enqueue(
        ocIds: Set<String>,
        manager: NSFileProviderManager,
        dbManager: FilesDatabaseManager,
        logger: FileProviderLogger
    ) {
        enqueue(
            ocIds: ocIds,
            ancestors: { dbManager.ancestorContainerIdentifiers(ofFileItemsWithOcIds: $0) },
            evictableState: { dbManager.hasEvictableDescendantFile(containerIdentifier: $0) },
            nudge: { try await manager.requestModification(of: [.lastUsedDate], forItemWithIdentifier: $0) },
            logger: logger
        )
    }

    /// Drop the batch and stop the drain, because its manager and database belong to the extension
    /// instance being invalidated.
    func cancel() {
        lock.lock()
        let task = drainTask
        drainTask = nil
        pendingOcIds.removeAll()
        lastNudgedEvictableState.removeAll()
        lock.unlock()

        task?.cancel()
    }

    /// One loop rather than one task per window, so a slow nudge delays the next drain instead of
    /// racing it.
    private func drain(
        generation: UInt64,
        ancestors: @Sendable (Set<String>) -> Set<NSFileProviderItemIdentifier>,
        evictableState: @Sendable (NSFileProviderItemIdentifier) -> Bool?,
        nudge: @Sendable (NSFileProviderItemIdentifier) async throws -> Void,
        logger: FileProviderLogger
    ) async {
        while true {
            try? await Task.sleep(nanoseconds: windowNanoseconds)

            if Task.isCancelled {
                break
            }

            await nudgeClaimedBatch(ancestors: ancestors, evictableState: evictableState, nudge: nudge, logger: logger)

            if Task.isCancelled {
                break
            }
            if endDrainIfIdle(generation: generation) {
                return
            }
        }

        endDrain(generation: generation)
    }

    private func nudgeClaimedBatch(
        ancestors: @Sendable (Set<String>) -> Set<NSFileProviderItemIdentifier>,
        evictableState: @Sendable (NSFileProviderItemIdentifier) -> Bool?,
        nudge: @Sendable (NSFileProviderItemIdentifier) async throws -> Void,
        logger: FileProviderLogger
    ) async {
        let claimed = claimBatch()

        guard !claimed.isEmpty else { return }

        let containers = ancestors(claimed)
        let changed = containersWithAChangedAnswer(among: containers, evictableState: evictableState)

        guard !changed.isEmpty else {
            logger.debug("No ancestor container of \(claimed.count) item(s) changed its Remove download answer; nothing to refresh.")
            return
        }

        logger.debug("Refreshing \(changed.count) of \(containers.count) ancestor container(s) for \(claimed.count) item(s) to update Remove download visibility.")

        for (container, state) in changed {
            do {
                try await nudge(container)
                // Recorded only once the nudge lands, so a failed one is retried next window.
                recordNudged(container, state: state)
            } catch {
                logger.error("Could not nudge ancestor container to refresh Remove download visibility.", [.item: container, .error: error.localizedDescription])
            }
        }
    }

    private func claimBatch() -> Set<String> {
        lock.lock()
        defer { lock.unlock() }

        let claimed = pendingOcIds
        pendingOcIds.removeAll()

        return claimed
    }

    /// The containers worth nudging, each with the state to record once its nudge lands, `nil` where
    /// there is nothing to record.
    private func containersWithAChangedAnswer(
        among containers: Set<NSFileProviderItemIdentifier>,
        evictableState: (NSFileProviderItemIdentifier) -> Bool?
    ) -> [(NSFileProviderItemIdentifier, Bool?)] {
        lock.lock()
        defer { lock.unlock() }

        return containers.compactMap { container in
            guard let state = evictableState(container) else { return (container, nil) }

            return lastNudgedEvictableState[container] == state ? nil : (container, state)
        }
    }

    private func recordNudged(_ container: NSFileProviderItemIdentifier, state: Bool?) {
        guard let state else { return }

        lock.lock()
        defer { lock.unlock() }

        lastNudgedEvictableState[container] = state
    }

    /// Ends the drain only if nothing arrived while it was nudging, so no work is left stranded.
    private func endDrainIfIdle(generation: UInt64) -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard pendingOcIds.isEmpty else { return false }

        if drainGeneration == generation {
            drainTask = nil
        }

        return true
    }

    private func endDrain(generation: UInt64) {
        lock.lock()
        defer { lock.unlock() }

        if drainGeneration == generation {
            drainTask = nil
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
/// Batched and filtered by ``AncestorRefreshCoalescer``, so callers are never blocked and a
/// container whose answer did not change is not nudged at all.
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

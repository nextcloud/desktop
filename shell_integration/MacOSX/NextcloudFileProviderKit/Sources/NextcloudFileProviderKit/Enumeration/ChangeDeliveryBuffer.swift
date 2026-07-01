//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// A thread-safe FIFO buffer of change metadata still to be delivered to an
/// `NSFileProviderChangeObserver` across the successive `enumerateChanges(for:from:)` invocations the
/// framework drives with `moreComing: true`.
///
/// The framework asserts (`__FILEPROVIDER_OBSERVER_TOO_MANY_ITEMS__`) when a single batch reports more
/// than 100× its suggested batch size — ≈20000 items for the default suggestion. Large change sets
/// (especially the working set, which aggregates the whole reachable tree) must therefore be split
/// across several batches, and the SDK resumes a split enumeration by *re-invoking* `enumerateChanges`
/// with the anchor that the previous batch returned — there is one `finishEnumeratingChanges` per call.
///
/// The change derivation that feeds this buffer is **destructive**: the working-set scan
/// (``Enumerator/scanMaterialisedItemsForRemoteChanges()``) and the depth-1 container read both persist
/// the discovered creations and updates (matching etags) to the local database as they run, so
/// re-deriving on a continuation invocation would surface (almost) no created/updated items and that
/// undelivered remainder would be silently dropped. The buffer therefore holds the full derived change
/// set after a single derivation and hands it out one batch at a time; the derivation never runs again
/// while the buffer stays primed for the same sync anchor.
///
/// Durability: the buffer is in memory. Within a normal `moreComing` chain the framework reuses the same
/// ``Enumerator`` instance, so the remainder survives. If the extension process is recycled mid-drain (only
/// reachable for change sets larger than ``Enumerator/maxBatchSize``), a fresh instance re-derives from the
/// original anchor: undelivered **deletions** are recovered (their soft-deleted rows persist with a fresh
/// `syncTime` until ``FilesDatabaseManager/removeItemMetadata(ocId:)`` runs after delivery, and the
/// working-set re-derivation re-surfaces them via `pendingWorkingSetChanges(since:)`), but undelivered
/// **created/updated** items can be lost — for a regular container entirely, and for the working set when
/// the item is not materialised. This replaces a guaranteed crash with a rare, bounded partial loss.
///
/// ``Enumerator`` is `Sendable` with only immutable members, so the mutable delivery state lives behind
/// this `@unchecked Sendable`, `NSLock`-guarded box — the established concurrency idiom in this target
/// (see `FileProviderExtension.actionsLock`). `Synchronization.Mutex` is unavailable because the
/// deployment target is macOS 13.
///
final class ChangeDeliveryBuffer: @unchecked Sendable {
    private let lock = NSLock()
    private let logger: FileProviderLogger
    private var anchorKey: String?
    private var remainingUpdated: [SendableItemMetadata] = []
    private var remainingDeleted: [SendableItemMetadata] = []

    init(log: any FileProviderLogging) {
        logger = FileProviderLogger(category: "ChangeDeliveryBuffer", log: log)
    }

    ///
    /// Whether a derivation has already filled the buffer for the given sync-anchor key and changes are
    /// still waiting to be delivered. A continuation invocation that sees `true` must drain rather than
    /// re-derive.
    ///
    func isPrimed(forKey key: String) -> Bool {
        lock.lock()
        defer { lock.unlock() }
        return anchorKey == key
    }

    ///
    /// Store the full derived change set for a drain sequence, keyed by the originating sync anchor.
    ///
    /// `updated` must already be sorted parents-before-children (ascending remote-path length) so that
    /// draining the single combined list front-to-back never reports a child in an earlier batch than
    /// its parent.
    ///
    func prime(key: String, updated: [SendableItemMetadata], deleted: [SendableItemMetadata]) {
        lock.lock()
        defer { lock.unlock() }
        anchorKey = key
        remainingUpdated = updated
        remainingDeleted = deleted
        logger.debug(
            "Primed change delivery buffer. anchor: \(key), updated: \(updated.count), deleted: \(deleted.count)"
        )
    }

    ///
    /// Pop the next batch, budgeting updates and deletions *together* against `maxItems` so the combined
    /// `didUpdate` + `didDeleteItems` payload of a single `finishEnumeratingChanges` stays under the cap.
    /// Updates are drained before deletions, preserving the parent-before-child ordering of the updates.
    ///
    /// `moreComing` reflects whether anything is still buffered *after* this batch (so an exactly-full
    /// final batch does not provoke a spurious empty follow-up). The key is cleared once both lists drain,
    /// so a later signal starts a fresh derivation.
    ///
    func takeBatch(
        maxItems: Int
    ) -> (updated: [SendableItemMetadata], deleted: [SendableItemMetadata], moreComing: Bool) {
        lock.lock()
        defer { lock.unlock() }

        let key = anchorKey ?? "nil"
        let budget = max(1, maxItems)

        let updatedCount = Swift.min(remainingUpdated.count, budget)
        let batchUpdated = Array(remainingUpdated.prefix(updatedCount))
        remainingUpdated.removeFirst(updatedCount)

        let deletedCount = Swift.min(remainingDeleted.count, budget - updatedCount)
        let batchDeleted = Array(remainingDeleted.prefix(deletedCount))
        remainingDeleted.removeFirst(deletedCount)

        let moreComing = !remainingUpdated.isEmpty || !remainingDeleted.isEmpty
        if !moreComing {
            anchorKey = nil
        }

        logger.debug(
            "Drained change batch. anchor: \(key), maxItems: \(budget), tookUpdated: \(updatedCount), tookDeleted: \(deletedCount), remainingUpdated: \(remainingUpdated.count), remainingDeleted: \(remainingDeleted.count), moreComing: \(moreComing)"
        )

        return (batchUpdated, batchDeleted, moreComing)
    }

    ///
    /// Discard any buffered state. Used when a fresh enumeration arrives under a different anchor than
    /// the one the buffer was primed for, so a stale remainder is never served against a new sync point.
    ///
    func reset() {
        lock.lock()
        defer { lock.unlock() }
        let previousKey = anchorKey
        let discardedUpdated = remainingUpdated.count
        let discardedDeleted = remainingDeleted.count

        anchorKey = nil
        remainingUpdated = []
        remainingDeleted = []

        // A non-empty discard means an in-progress drain was abandoned (a fresh enumeration arrived under
        // a different anchor). Log it loudly enough to spot potential change loss from a debug archive.
        if discardedUpdated > 0 || discardedDeleted > 0 {
            logger.info(
                "Reset change delivery buffer, discarding undelivered remainder. anchor: \(previousKey ?? "nil"), discardedUpdated: \(discardedUpdated), discardedDeleted: \(discardedDeleted)"
            )
        }
    }
}

//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKit

public enum MockChangeObserverError: Error {
    /// The enumerator signalled completion without reporting a finish, which should not be reachable.
    case finishedWithoutReporting
}

public class MockChangeObserver: NSObject, NSFileProviderChangeObserver, @unchecked Sendable {
    public var changedItems: [any NSFileProviderItemProtocol] = []
    public var deletedItemIdentifiers: [NSFileProviderItemIdentifier] = []
    public var didDeleteItemsHandler: (([NSFileProviderItemIdentifier]) -> Void)?
    /// Every `finishEnumeratingChanges` call recorded as `(anchor, moreComing)`, in order, so tests can
    /// assert the batching behaviour (intermediate vs final anchors, batch count).
    public private(set) var finishes: [(anchor: NSFileProviderSyncAnchor, moreComing: Bool)] = []
    /// The cumulative number of reported items (updates + deletions) observed at each finish, so tests can
    /// derive per-batch sizes and assert no batch exceeds the cap. `didUpdate`/`didDeleteItems` for a
    /// batch are delivered before its `finishEnumeratingChanges`, so each entry includes that batch.
    public private(set) var reportedCountsAtFinish: [Int] = []
    /// Whether each `finishEnumeratingChanges` arrived on the main thread, in order.
    ///
    /// Production acknowledges a batch immediately after reporting it finished, in the same job and
    /// without suspending, so the acknowledgement — the soft-delete and hard-remove writes — is ordered
    /// only for callers that can get behind that job. ``enumerateChangesBatch(from:)`` does that by
    /// hopping to the MainActor, which works precisely because the batch job runs there. A batch
    /// reported off the main thread is therefore a race the harness cannot close, not a stylistic
    /// detail, and tests assert on this to keep it from coming back.
    public private(set) var finishesDeliveredOnMainThread: [Bool] = []
    /// Optional synchronous hook invoked immediately before a change batch is finished.
    public var beforeFinishEnumeratingChanges: (() -> Void)?
    /// Mirrors the system-set `suggestedBatchSize`. `@objc` so the optional protocol requirement is seen
    /// by the production code through the protocol existential; set small to force multi-batch delivery.
    @objc public var suggestedBatchSize: Int = 0
    var error: Error?
    var isComplete = false
    private var batchComplete = false
    var enumerator: NSFileProviderEnumerator

    public init(enumerator: NSFileProviderEnumerator) {
        self.enumerator = enumerator
    }

    public func didUpdate(_ changedItems: [any NSFileProviderItemProtocol]) {
        self.changedItems.append(contentsOf: changedItems)
    }

    public func didDeleteItems(withIdentifiers deletedItemIdentifiers: [NSFileProviderItemIdentifier]) {
        self.deletedItemIdentifiers.append(contentsOf: deletedItemIdentifiers)
        didDeleteItemsHandler?(deletedItemIdentifiers)
    }

    public func finishEnumeratingChanges(upTo anchor: NSFileProviderSyncAnchor, moreComing: Bool) {
        beforeFinishEnumeratingChanges?()
        finishes.append((anchor, moreComing))
        reportedCountsAtFinish.append(changedItems.count + deletedItemIdentifiers.count)
        finishesDeliveredOnMainThread.append(Thread.isMainThread)
        // moreComing: the framework would re-invoke enumerateChanges from this anchor for the next batch.
        isComplete = !moreComing
        batchComplete = true
    }

    public func finishEnumeratingWithError(_ error: Error) {
        self.error = error
        isComplete = true
        batchComplete = true
    }

    ///
    /// Drive exactly one batch of changes from `anchor` and return how it finished, once the batch has
    /// been acknowledged.
    ///
    /// Always drive change enumeration through this method or ``enumerateChanges(from:)``, never by
    /// calling `enumerator.enumerateChanges(for:from:)` and polling for a result: production code
    /// acknowledges a batch *after* it reports the batch finished, and the acknowledgement is what
    /// writes the soft-deletes and hard-removes. A hand-rolled poll loop resumes in between and races
    /// those writes.
    ///
    @discardableResult
    public func enumerateChangesBatch(from anchor: NSFileProviderSyncAnchor) async throws
        -> (anchor: NSFileProviderSyncAnchor, moreComing: Bool)
    {
        batchComplete = false
        enumerator.enumerateChanges?(for: self, from: anchor)

        while !batchComplete {
            try await Task.sleep(nanoseconds: 1_000_000)
        }

        // `batchComplete` is set inside `finishEnumeratingChanges`, but the production batch job goes
        // on to call `changeBuffer.acknowledgeBatch(...)` — the soft-delete and hard-remove writes — in
        // the same MainActor job, without suspending in between. Hopping to the MainActor here
        // therefore waits for that job to finish, so the caller's assertions and the next batch both
        // see an acknowledged database rather than racing it.
        await MainActor.run {}

        if let error {
            throw error
        }

        guard let finish = finishes.last else {
            throw MockChangeObserverError.finishedWithoutReporting
        }

        return finish
    }

    public func enumerateChanges(from anchor: NSFileProviderSyncAnchor =
        Enumerator.syncAnchor(at: Date(timeIntervalSince1970: 1))) async throws
    {
        isComplete = false
        var currentAnchor = anchor
        // Drive the batches the way the framework does: re-invoke enumerateChanges from the anchor the
        // previous batch returned until one finishes with moreComing == false.
        repeat {
            let finish = try await enumerateChangesBatch(from: currentAnchor)
            currentAnchor = finish.anchor
        } while !isComplete
    }

    public func reset() {
        changedItems = []
        deletedItemIdentifiers = []
        finishes = []
        reportedCountsAtFinish = []
        finishesDeliveredOnMainThread = []
        error = nil
        isComplete = false
        batchComplete = false
    }
}

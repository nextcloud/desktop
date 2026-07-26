//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKit

public class MockChangeObserver: NSObject, NSFileProviderChangeObserver {
    public var changedItems: [any NSFileProviderItemProtocol] = []
    public var deletedItemIdentifiers: [NSFileProviderItemIdentifier] = []
    /// Every `finishEnumeratingChanges` call recorded as `(anchor, moreComing)`, in order, so tests can
    /// assert the batching behaviour (intermediate vs final anchors, batch count).
    public private(set) var finishes: [(anchor: NSFileProviderSyncAnchor, moreComing: Bool)] = []
    /// The cumulative number of reported items (updates + deletions) observed at each finish, so tests can
    /// derive per-batch sizes and assert no batch exceeds the cap. `didUpdate`/`didDeleteItems` for a
    /// batch are delivered before its `finishEnumeratingChanges`, so each entry includes that batch.
    public private(set) var reportedCountsAtFinish: [Int] = []
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
    }

    public func finishEnumeratingChanges(upTo anchor: NSFileProviderSyncAnchor, moreComing: Bool) {
        finishes.append((anchor, moreComing))
        reportedCountsAtFinish.append(changedItems.count + deletedItemIdentifiers.count)
        // moreComing: the framework would re-invoke enumerateChanges from this anchor for the next batch.
        isComplete = !moreComing
        batchComplete = true
    }

    public func finishEnumeratingWithError(_ error: Error) {
        self.error = error
        isComplete = true
        batchComplete = true
    }

    public func enumerateChanges(from anchor: NSFileProviderSyncAnchor =
        Enumerator.syncAnchor(at: Date(timeIntervalSince1970: 1))) async throws
    {
        isComplete = false
        var currentAnchor = anchor
        // Drive the batches the way the framework does: re-invoke enumerateChanges from the anchor the
        // previous batch returned until one finishes with moreComing == false.
        repeat {
            batchComplete = false
            enumerator.enumerateChanges?(for: self, from: currentAnchor)
            while !batchComplete {
                try await Task.sleep(nanoseconds: 1_000_000)
            }
            if let error {
                throw error
            }
            if let latestAnchor = finishes.last?.anchor {
                currentAnchor = latestAnchor
            }
        } while !isComplete
    }

    public func reset() {
        changedItems = []
        deletedItemIdentifiers = []
        finishes = []
        reportedCountsAtFinish = []
        error = nil
        isComplete = false
        batchComplete = false
    }
}

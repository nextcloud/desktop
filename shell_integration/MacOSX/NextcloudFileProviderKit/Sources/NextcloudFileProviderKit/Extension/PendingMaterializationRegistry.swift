//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation

///
/// The set of items this process has just downloaded and marked `downloaded = true` in the
/// database, but which the system has not yet reported back through
/// `NSFileProviderManager.enumeratorForMaterializedItems()`, and which
/// ``MaterializedEnumerationObserver`` must therefore not reconcile as evicted.
///
final class PendingMaterializationRegistry: @unchecked Sendable {
    static let shared = PendingMaterializationRegistry()

    ///
    /// How long an unconfirmed download is protected from being reconciled as evicted, a backstop
    /// for a download the system never confirms rather than the normal exit.
    ///
    private static let expiryInterval: TimeInterval = 60

    private let lock = NSLock()
    private var pending = [NSFileProviderItemIdentifier: Date]()

    ///
    /// Record that this process just materialised `identifier` locally and wrote that to the
    /// database, ahead of the system's own bookkeeping.
    ///
    func recordDownloaded(_ identifier: NSFileProviderItemIdentifier, at date: Date = Date()) {
        lock.lock()
        defer { lock.unlock() }
        pending[identifier] = date
    }

    ///
    /// Note that the system has now enumerated `identifier` as materialized, so the entry has
    /// served its purpose and later evictions of it must be honoured immediately.
    ///
    func confirmMaterialized(_ identifier: NSFileProviderItemIdentifier) {
        lock.lock()
        defer { lock.unlock() }
        pending.removeValue(forKey: identifier)
    }

    ///
    /// The subset of `identifiers` whose download is still awaiting confirmation and which must
    /// therefore not be reconciled as evicted yet, pruning lapsed entries as a side effect.
    ///
    func awaitingConfirmation(
        among identifiers: Set<NSFileProviderItemIdentifier>,
        now: Date = Date()
    ) -> Set<NSFileProviderItemIdentifier> {
        lock.lock()
        defer { lock.unlock() }

        pending = pending.filter { now.timeIntervalSince($0.value) < Self.expiryInterval }

        return identifiers.filter { pending[$0] != nil }
    }

    ///
    /// Drop all state, a test seam because the registry is a process-wide singleton.
    ///
    func removeAll() {
        lock.lock()
        defer { lock.unlock() }
        pending.removeAll()
    }
}

//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation

///
/// The set of items this process has just downloaded and marked `downloaded = true` in the
/// database, but which the system has not yet reported back through
/// `NSFileProviderManager.enumeratorForMaterializedItems()`.
///
/// ## Why this exists
///
/// ``Item/fetchContents(domain:progress:dbManager:)`` writes `downloaded = true` and returns the
/// content URL; the framework materialises the file and adds it to its materialized set only
/// *afterwards*. ``MaterializedEnumerationObserver`` reconciles in the opposite direction — it
/// assumes every materialized database row is evicted until the system's enumeration proves
/// otherwise — so any item caught in that window was flipped straight back to
/// `downloaded = false` and logged as `Updating item state to dataless.`. The framework then
/// re-requested the content, which downloaded it again, which reopened the window. During a bulk
/// materialisation of a "keep downloaded" tree this produced a self-sustaining loop (132,165
/// dataless transitions in a single extension log).
///
/// Recording the download here closes the window: the reconciliation skips an item until the
/// system has confirmed it, and confirmation removes the entry so ordinary evictions are still
/// detected on the very next pass.
///
/// ## Lifetime
///
/// The race is process-local by construction — it is the gap between this process' database write
/// and this process' next callback — so in-memory state is the right scope and no Realm migration
/// is involved. ``expiryInterval`` is only a backstop for a download the system never confirms
/// (for example because the extension is torn down mid-transfer); the normal exit is
/// ``confirmMaterialized(_:)``.
///
/// Guarded by an `NSLock` rather than an `actor` because
/// ``MaterializedEnumerationObserver/handleEnumeratedItems(_:account:dbManager:completionHandler:)``
/// runs synchronously on the framework's callback thread. Same idiom as ``ChangeDeliveryBuffer``.
///
final class PendingMaterializationRegistry: @unchecked Sendable {
    static let shared = PendingMaterializationRegistry()

    ///
    /// How long an unconfirmed download is protected from being reconciled as evicted.
    ///
    /// Generous on purpose: being too short reopens the flip-flop, whereas being too long only
    /// delays the dataless marking of an item evicted immediately after download until the entry
    /// lapses. Confirmation normally clears entries long before this.
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
    /// therefore not be reconciled as evicted yet. Lapsed entries are pruned as a side effect.
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
    /// Drop all state. Test seam — the registry is a process-wide singleton, so a test that
    /// exercises the reconciliation must not inherit entries from an earlier one.
    ///
    func removeAll() {
        lock.lock()
        defer { lock.unlock() }
        pending.removeAll()
    }
}

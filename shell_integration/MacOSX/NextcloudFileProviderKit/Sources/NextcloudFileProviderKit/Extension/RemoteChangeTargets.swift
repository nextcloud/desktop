//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// Containers a `notify_push` message named as changed, waiting to be scanned.
///
/// ## Why
///
/// A push already tells us exactly what changed, within a second or so. The extension used to treat
/// it as a bare doorbell — `processFileIdsChanged` checked whether *any* of the ids were locally
/// known, threw the ids away, and signalled the whole working set. That turned a one-file change
/// into a walk of every materialised item: one measured pass took 305 seconds to surface a single
/// 31-byte file the push had identified 1.5 seconds after it was created.
///
/// Recording the ids instead lets the working-set derivation read just those containers. A depth-1
/// read of the parent is what reveals a new child, a modified child and a removed child alike, so
/// one narrow read covers every case the wide walk did — for the items the push actually named.
///
/// ## Why the full scan stays
///
/// Push is not a guarantee. Messages are lost across reconnects and the socket can be down entirely,
/// and the server only propagates etags up the tree — a push tells us a subtree changed, not that
/// nothing else did. Targeted scans are therefore an accelerator layered over the periodic full
/// walk, never a replacement: ``shouldRunFullScan(now:)`` forces one whenever nothing is targeted or
/// ``fullScanInterval`` has elapsed, so anything push missed is still reconciled.
///
/// Guarded by an `NSLock` and process-wide, matching ``PendingMaterializationRegistry``. One
/// extension process serves one domain.
///
final class RemoteChangeTargets: @unchecked Sendable {
    static let shared = RemoteChangeTargets()

    /// How long a targeted-only run may go before a full reconciliation is forced anyway.
    static let fullScanInterval: TimeInterval = 10 * 60

    private let lock = NSLock()
    private var pending = Set<NSFileProviderItemIdentifier>()
    private var lastFullScan: Date?

    ///
    /// Note that a push named these containers as changed.
    ///
    /// Callers pass containers, not the changed items themselves: for a file that is its parent
    /// directory, whose depth-1 read shows the file's new state or its absence.
    ///
    func record(containers: some Sequence<NSFileProviderItemIdentifier>) {
        lock.lock()
        defer { lock.unlock() }
        pending.formUnion(containers)
    }

    ///
    /// Take the containers accumulated since the last derivation, clearing them.
    ///
    /// Returns `nil` when nothing is pending, which the caller reads as "no targeting information —
    /// fall back to the full walk".
    ///
    func consumeTargets() -> Set<NSFileProviderItemIdentifier>? {
        lock.lock()
        defer { lock.unlock() }

        guard !pending.isEmpty else { return nil }

        let targets = pending
        pending.removeAll()
        return targets
    }

    ///
    /// Whether this derivation must be a full walk rather than a targeted one.
    ///
    /// True until the first full scan has run, and again once ``fullScanInterval`` has elapsed since
    /// it — so a long stream of pushes can never postpone reconciliation indefinitely.
    ///
    func shouldRunFullScan(now: Date = Date()) -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard let lastFullScan else { return true }

        return now.timeIntervalSince(lastFullScan) >= Self.fullScanInterval
    }

    /// Record that a full walk just completed, restarting the interval.
    func noteFullScanCompleted(at date: Date = Date()) {
        lock.lock()
        defer { lock.unlock() }
        lastFullScan = date
    }

    /// Drop all state. Test seam — the registry is a process-wide singleton.
    func removeAll() {
        lock.lock()
        defer { lock.unlock() }
        pending.removeAll()
        lastFullScan = nil
    }
}

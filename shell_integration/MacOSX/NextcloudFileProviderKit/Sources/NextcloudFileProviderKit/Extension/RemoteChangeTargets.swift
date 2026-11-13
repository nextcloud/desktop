//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// The items that pushes named as changed, and whether the next working-set scan must walk the whole materialised set instead.
///
public final class RemoteChangeTargets: @unchecked Sendable {
    ///
    /// What the next working-set scan reads.
    ///
    enum Scope: Equatable {
        case fullWalk
        case targets(Set<NSFileProviderItemIdentifier>)
    }

    /// Push can drop messages across reconnects, so the first derivation this long after the start of the last complete full walk walks everything.
    static let fullWalkInterval: TimeInterval = 10 * 60

    private let lock = NSLock()
    private var pendingTargets = Set<NSFileProviderItemIdentifier>()
    private var fullWalkRequested = false
    private var lastFullWalk: Date?

    public init() {}

    ///
    /// Add items to read in the next working-set scan.
    ///
    func record(targets: some Sequence<NSFileProviderItemIdentifier>) {
        lock.withLock {
            pendingTargets.formUnion(targets)
        }
    }

    ///
    /// Make the next working-set scan a full walk.
    ///
    func requestFullWalk() {
        lock.withLock {
            fullWalkRequested = true
        }
    }

    ///
    /// Take what the next working-set scan reads, clearing the pending targets and any full-walk request.
    ///
    /// - Parameter now: The time compared against ``fullWalkInterval``.
    ///
    func takeScope(now: Date = Date()) -> Scope {
        lock.withLock {
            defer {
                pendingTargets.removeAll()
                fullWalkRequested = false
            }

            // A clock set back makes the interval negative, which counts as elapsed.
            guard !pendingTargets.isEmpty, !fullWalkRequested, let lastFullWalk, (0 ..< Self.fullWalkInterval).contains(now.timeIntervalSince(lastFullWalk)) else {
                return .fullWalk
            }

            return .targets(pendingTargets)
        }
    }

    ///
    /// Restart the interval after a complete full walk.
    ///
    /// - Parameter date: When that walk started, because a change during the walk may have been missed by a read made before it.
    ///
    func noteFullWalkCompleted(startedAt date: Date) {
        lock.withLock {
            lastFullWalk = max(lastFullWalk ?? date, date)
        }
    }
}

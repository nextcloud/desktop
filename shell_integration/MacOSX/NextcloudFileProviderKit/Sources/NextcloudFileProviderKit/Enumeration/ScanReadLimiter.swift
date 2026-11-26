//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Keeps the listings one working-set pass has in flight at or below a limit.
///
/// The walk reads the children of a directory together, and each of those reads its own children, so the
/// number of listings in flight would otherwise grow with the depth of the tree. The default limit stays
/// below NextcloudKit's per-host connection limit on the data session, which downloads and uploads share, so
/// against a server speaking HTTP/1.1 a pass leaves connections to the user's own transfers.
///
/// A slot is held for one listing, never while a subtree below it is walked, so a chain deeper than the
/// limit cannot wait for itself. The stored parents a listing reads to tell a move from a second mount are
/// read one at a time under its slot, so a pass has no more requests out than it has slots.
///
actor ScanReadLimiter {
    private let limit: Int
    private var inFlight = 0
    private var waiting: [CheckedContinuation<Void, Never>] = []

    init(limit: Int = workingSetScanConcurrency) {
        self.limit = max(1, limit)
    }

    /// The listings in flight right now, for tests.
    var currentlyInFlight: Int {
        inFlight
    }

    /// Wait until a slot is free. Callers are served in the order they arrived.
    func acquire() async {
        guard inFlight >= limit else {
            inFlight += 1
            return
        }

        await withCheckedContinuation { continuation in
            waiting.append(continuation)
        }
    }

    /// Hand the slot to whoever waited longest, or give it up.
    func release() {
        guard waiting.isEmpty else {
            waiting.removeFirst().resume()
            return
        }

        inFlight -= 1
    }
}

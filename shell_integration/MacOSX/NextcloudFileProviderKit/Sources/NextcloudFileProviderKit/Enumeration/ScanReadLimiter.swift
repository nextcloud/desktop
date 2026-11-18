//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Keeps the listings one working-set pass has in flight at or below a limit.
///
/// The walk reads the children of a directory together, and each of those reads its own children, so the
/// number of listings in flight would otherwise grow with the depth of the tree. Six is NextcloudKit's
/// default per-host connection limit on the data session, which downloads and uploads share, so a pass
/// above it would queue the user's own transfers behind its listings.
///
/// A slot is held for one listing, never while a subtree below it is walked, so a chain deeper than the
/// limit cannot wait for itself.
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

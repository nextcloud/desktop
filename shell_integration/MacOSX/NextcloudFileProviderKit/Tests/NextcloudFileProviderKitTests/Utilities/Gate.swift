//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

/// Parks every caller of ``wait()`` until the test calls ``release()``, so the test decides when an awaited step continues.
actor Gate {
    /// How many times ``wait()`` was entered, including callers already released.
    private(set) var entries = 0

    private var parked: [CheckedContinuation<Void, Never>] = []

    func wait() async {
        entries += 1

        await withCheckedContinuation { continuation in
            parked.append(continuation)
        }
    }

    /// Let the callers parked so far continue, while later callers park again.
    func release() {
        let released = parked
        parked.removeAll()

        for continuation in released {
            continuation.resume()
        }
    }
}

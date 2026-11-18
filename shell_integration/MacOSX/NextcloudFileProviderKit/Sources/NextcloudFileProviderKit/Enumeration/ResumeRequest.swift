//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// A working-set pass which stopped at its listing budget asks for the next pass once its last batch is acknowledged; see ``WorkingSetScanLimits``.
///
/// The request lives with the enumerator instance. One replaced while its batches are still being drained forgets it; the rest then waits for the next signal from the app.
///
final class ResumeRequest: @unchecked Sendable {
    private let lock = NSLock()
    private var requested = false

    func request() {
        lock.withLock { requested = true }
    }

    /// Whether a resume was requested; clears it.
    func consume() -> Bool {
        lock.withLock {
            defer { requested = false }
            return requested
        }
    }
}

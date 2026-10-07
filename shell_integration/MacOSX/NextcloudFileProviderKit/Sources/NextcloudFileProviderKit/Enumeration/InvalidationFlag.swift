//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// A flag which is set once, from any thread, and read from any thread.
///
/// ``Enumerator`` keeps one so that work which finishes after the framework invalidated the enumerator can tell, for example to leave items unmarked which the system may never ingest.
///
final class InvalidationFlag: @unchecked Sendable {
    private let lock = NSLock()
    private var value = false

    var isSet: Bool {
        lock.withLock { value }
    }

    func set() {
        lock.withLock { value = true }
    }
}

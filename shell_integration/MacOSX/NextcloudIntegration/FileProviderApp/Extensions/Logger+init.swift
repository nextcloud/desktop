//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import os

extension Logger {
    ///
    /// Convenience initializer to reduce the arguments to the category and populate the subsystem consistently.
    ///
    init(category: String) {
        self.init(OSLog(subsystem: Bundle.main.bundleIdentifier!, category: category))
    }
}

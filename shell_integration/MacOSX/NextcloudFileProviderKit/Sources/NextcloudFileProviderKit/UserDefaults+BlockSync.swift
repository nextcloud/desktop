//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

extension UserDefaults {
    ///
    /// KVO-observable accessor for the process-global `blockSync` user default.
    ///
    /// The `@objc dynamic` attribute is required so `FileProviderExtension` can observe the key via `\.blockSync` and notice the moment synchronization is unblocked by `defaults write` without restarting the extension.
    /// The gates which refuse work read the value directly instead of observing it, so this exists only to catch the edge on which throttled items have to be released.
    /// `NSNumber?` is used instead of `Bool` so observers can distinguish a reset (key absent) from an explicit `false`.
    ///
    @objc dynamic var blockSync: NSNumber? {
        object(forKey: "blockSync") as? NSNumber
    }
}

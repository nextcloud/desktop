//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

extension UserDefaults {
    ///
    /// KVO-observable accessor for the process-global `debugLoggingEnabled` user default.
    ///
    /// The `@objc dynamic` attribute is required so `FileProviderLog` can observe the key via `\.debugLoggingEnabled` and react to changes made by `defaults write` without restarting the extension.
    /// `NSNumber?` is used instead of `Bool` so observers can distinguish a reset (key absent) from an explicit `false`.
    ///
    @objc dynamic var debugLoggingEnabled: NSNumber? {
        object(forKey: "debugLoggingEnabled") as? NSNumber
    }
}

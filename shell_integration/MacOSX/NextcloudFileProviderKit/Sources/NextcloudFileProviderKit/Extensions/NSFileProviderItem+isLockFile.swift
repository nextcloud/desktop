//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

extension NSFileProviderItem {
    ///
    /// Whether the item is a lock file as created by certain applications like Microsoft Office or LibreOffice.
    ///
    /// This is detected based on the file name which must match a certain pattern.
    ///
    public var isLockFile: Bool {
        // Microsoft Office || LibreOffice
        filename.hasPrefix("~$") || (filename.hasPrefix(".~lock.") && filename.hasSuffix("#"))
    }
}

//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider

extension Item: NSFileProviderItemDecorating {
    ///
    /// Finder overlay badges declared by the extension's Info.plist.
    ///
    public var decorations: [NSFileProviderItemDecorationIdentifier]? {
        guard metadata.keepDownloaded else {
            return nil
        }

        guard let identifier = Bundle.main.bundleIdentifier else {
            return nil
        }

        return [
            NSFileProviderItemDecorationIdentifier(rawValue: "\(identifier).keep-downloaded")
        ]
    }
}

//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

extension NSFileProviderDomain {
    ///
    /// Mock a random object of this type for tests and previews.
    ///
    static var mock: NSFileProviderDomain {
        let uuid = UUID()
        let identifier = NSFileProviderDomainIdentifier(uuid.uuidString)

        return NSFileProviderDomain(identifier: identifier, displayName: uuid.uuidString)
    }
}

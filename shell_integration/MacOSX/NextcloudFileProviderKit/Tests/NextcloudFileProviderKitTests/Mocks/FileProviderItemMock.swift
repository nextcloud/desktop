//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

final class MockFileProviderItem: NSObject, NSFileProviderItemProtocol {
    var itemIdentifier: NSFileProviderItemIdentifier
    var filename: String
    var isUploaded: Bool

    init(identifier: NSFileProviderItemIdentifier, filename: String, isUploaded: Bool) {
        itemIdentifier = identifier
        self.filename = filename
        self.isUploaded = isUploaded
    }

    var parentItemIdentifier: NSFileProviderItemIdentifier {
        .rootContainer
    }

    var capabilities: NSFileProviderItemCapabilities {
        []
    }
}

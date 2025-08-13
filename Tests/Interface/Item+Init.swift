//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks

public extension Item {
    convenience init(
        metadata: SendableItemMetadata,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager
    ) {
        self.init(
            metadata: metadata,
            parentItemIdentifier: parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
    }
}

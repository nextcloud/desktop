//
//  Item+Init.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 27/5/25.
//

import FileProvider
import Foundation
import NextcloudFileProviderKit

public extension Item {
    convenience init(
        metadata: SendableItemMetadata,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
    ) {
        self.init(
            metadata: metadata,
            parentItemIdentifier: parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: true
        )
    }
}

//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

extension NSFileProviderError {
    func handlingCollisionAgainstItemInRemotePath(
        _ problemRemotePath: String,
        dbManager: FilesDatabaseManager,
        remoteInterface: RemoteInterface,
        log: any FileProviderLogging
    ) async -> Error {
        guard code == .filenameCollision else {
            return self
        }
        let account = dbManager.account
        if let collidingItemMetadata = dbManager.itemMetadata(
            account: account.ncKitAccount,
            locatedAtRemoteUrl: problemRemotePath
        ), let collidingItem = await Item.storedItem(
            identifier: .init(collidingItemMetadata.ocId),
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            log: log
        ) {
            return NSError.fileProviderErrorForCollision(with: collidingItem)
        }
        return self
    }

    static func collision(
        with metadata: SendableItemMetadata?,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        dbManager: FilesDatabaseManager,
        remoteInterface: RemoteInterface,
        log: any FileProviderLogging
    ) -> Error {
        guard let metadata else { return NSFileProviderError(.filenameCollision) }
        // Enumeration must still see an unknown item as new when it reports it to File Provider.
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: parentItemIdentifier,
            account: dbManager.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: false,
            remoteSupportsTrash: false,
            log: log
        )
        return NSError.fileProviderErrorForCollision(with: item)
    }
}

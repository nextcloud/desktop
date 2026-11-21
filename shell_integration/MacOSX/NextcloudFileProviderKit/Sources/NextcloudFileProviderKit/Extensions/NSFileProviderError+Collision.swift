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
        guard let collidingItemMetadata = dbManager.itemMetadata(
            account: dbManager.account.ncKitAccount, locatedAtRemoteUrl: problemRemotePath
        ), let collidingItem = await Item.storedItem(
            identifier: .init(collidingItemMetadata.ocId),
            account: dbManager.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            log: log
        ) else {
            return self
        }
        return NSError.fileProviderErrorForCollision(with: collidingItem)
    }
}

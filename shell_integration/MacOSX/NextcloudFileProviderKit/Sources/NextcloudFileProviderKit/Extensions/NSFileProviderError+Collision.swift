//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit

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
        atRemotePath remotePath: String,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        sourceMetadata: SendableItemMetadata?,
        dbManager: FilesDatabaseManager,
        remoteInterface: RemoteInterface,
        progress: Progress,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void,
        log: any FileProviderLogging
    ) async -> Error {
        let collision = NSFileProviderError(.filenameCollision)
        let account = dbManager.account
        guard !remotePath.contains(FilesDatabaseManager.holdingNameSeparator),
              let relativePath = account.filesRootRelativePath(for: remotePath, sourceMetadata: sourceMetadata)
        else {
            return collision
        }
        return await NetworkOperationCancellation(log: log).run(progress: progress) { cancellation -> any Error in
            let registerTask: @Sendable (URLSessionTask) -> Void = { task in
                cancellation.register(task: task)
                taskHandler(task)
            }
            let isCancelled: @Sendable () -> Bool = { cancellation.isCancelled || Task.isCancelled }
            guard !isCancelled() else {
                return CocoaError(.userCancelled)
            }
            let (_, files, _, error) = await remoteInterface.enumerate(
                remotePath: remotePath,
                depth: .target,
                showHiddenFiles: true,
                includeHiddenFiles: [],
                requestBody: nil,
                account: account,
                options: .init(),
                taskHandler: registerTask
            )
            guard !isCancelled() else {
                return CocoaError(.userCancelled)
            }
            guard error == .success,
                  let metadata = files.first?.toItemMetadata(),
                  !metadata.ocId.isEmpty,
                  metadata.account == account.ncKitAccount,
                  let responsePath = account.filesRootRelativePath(for: metadata.remotePath(), sourceMetadata: metadata),
                  responsePath.utf8.elementsEqual(relativePath.utf8)
            else {
                return collision
            }
            // Enumeration must still see an unknown item as new when it reports it to File Provider.
            let item = Item(
                metadata: metadata,
                parentItemIdentifier: parentItemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                displayFileActions: false,
                remoteSupportsTrash: false,
                log: log
            )
            return NSError.fileProviderErrorForCollision(with: item)
        }
    }
}

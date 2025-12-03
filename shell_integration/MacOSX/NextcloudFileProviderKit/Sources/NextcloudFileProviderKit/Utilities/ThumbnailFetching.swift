//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit

///
/// Stateless function to fetch thumbnails from the server.
///
/// > To Do: This needs to become part of the type implementing `NSFileProviderReplicatedExtension` once it is moved from the desktop client into this package.
///
public func fetchThumbnails(
    for itemIdentifiers: [NSFileProviderItemIdentifier],
    requestedSize size: CGSize,
    account: Account,
    usingRemoteInterface remoteInterface: RemoteInterface,
    andDatabase dbManager: FilesDatabaseManager,
    perThumbnailCompletionHandler: @Sendable @escaping (
        NSFileProviderItemIdentifier,
        Data?,
        Error?
    ) -> Void,
    log: any FileProviderLogging,
    completionHandler: @Sendable @escaping (Error?) -> Void
) -> Progress {
    let logger = FileProviderLogger(category: "fetchThumbnails", log: log)
    let progress = Progress(totalUnitCount: Int64(itemIdentifiers.count))

    @Sendable func finishCurrent() {
        progress.completedUnitCount += 1

        if progress.completedUnitCount == progress.totalUnitCount {
            completionHandler(nil)
        }
    }

    for itemIdentifier in itemIdentifiers {
        Task {
            guard let item = await Item.storedItem(
                identifier: itemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                log: logger.log
            ) else {
                logger.error("Could not find item, unable to download thumbnail!", [.item: itemIdentifier.rawValue])

                perThumbnailCompletionHandler(
                    itemIdentifier,
                    nil,
                    NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier)
                )
                finishCurrent()
                return
            }

            let (data, error) = await item.fetchThumbnail(size: size)
            perThumbnailCompletionHandler(itemIdentifier, data, error)
            finishCurrent()
        }
    }

    return progress
}

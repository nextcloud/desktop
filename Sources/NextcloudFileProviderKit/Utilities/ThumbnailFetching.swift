//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudKit
import OSLog

fileprivate let logger = Logger(subsystem: Logger.subsystem, category: "thumbnails")

public func fetchThumbnails(
    for itemIdentifiers: [NSFileProviderItemIdentifier],
    requestedSize size: CGSize,
    account: Account,
    usingRemoteInterface remoteInterface: RemoteInterface,
    andDatabase dbManager: FilesDatabaseManager,
    perThumbnailCompletionHandler: @escaping (
        NSFileProviderItemIdentifier,
        Data?,
        Error?
    ) -> Void,
    completionHandler: @Sendable @escaping (Error?) -> Void
) -> Progress {
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
                dbManager: dbManager
            ) else {
                logger.error(
                    """
                    Could not find item with identifier: \(itemIdentifier.rawValue, privacy: .public),
                    unable to download thumbnail!
                    """
                )
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

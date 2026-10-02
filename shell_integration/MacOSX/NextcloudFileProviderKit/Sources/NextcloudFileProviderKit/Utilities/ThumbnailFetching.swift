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
    domain: NSFileProviderDomain? = nil,
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

    guard !itemIdentifiers.isEmpty else {
        completionHandler(nil)
        return progress
    }

    Task { @Sendable in
        let error: Error? = await NetworkOperationCancellation(log: log).run(progress: progress) { @Sendable _ in
            await withTaskGroup(of: Void.self) { group in
                for itemIdentifier in itemIdentifiers {
                    let childProgress = Progress(totalUnitCount: 1)
                    progress.addChild(childProgress, withPendingUnitCount: 1)
                    group.addTask { @Sendable in
                        defer { childProgress.completedUnitCount = 1 }
                        guard !progress.isCancelled, !Task.isCancelled else {
                            perThumbnailCompletionHandler(itemIdentifier, nil, CocoaError(.userCancelled))
                            return
                        }
                        let item = await Item.storedItem(
                            identifier: itemIdentifier,
                            account: account,
                            remoteInterface: remoteInterface,
                            dbManager: dbManager,
                            log: logger.log
                        )
                        guard !progress.isCancelled, !Task.isCancelled else {
                            perThumbnailCompletionHandler(itemIdentifier, nil, CocoaError(.userCancelled))
                            return
                        }
                        guard let item else {
                            logger.error("Could not find item, unable to download thumbnail.", [.item: itemIdentifier.rawValue])
                            perThumbnailCompletionHandler(itemIdentifier, nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier))
                            return
                        }
                        let (data, error) = await item.fetchThumbnail(size: size, domain: domain, progress: childProgress)
                        if progress.isCancelled || Task.isCancelled {
                            perThumbnailCompletionHandler(itemIdentifier, nil, CocoaError(.userCancelled))
                        } else {
                            perThumbnailCompletionHandler(itemIdentifier, data, error)
                        }
                    }
                }
            }
            return progress.isCancelled || Task.isCancelled ? CocoaError(.userCancelled) : nil
        }
        completionHandler(error)
    }

    return progress
}

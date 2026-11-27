//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit
import OSLog

extension FileProviderExtension: NSFileProviderThumbnailing {
    public func fetchThumbnails(
        for itemIdentifiers: [NSFileProviderItemIdentifier],
        requestedSize size: CGSize,
        perThumbnailCompletionHandler: @escaping (
            NSFileProviderItemIdentifier,
            Data?,
            Error?
        ) -> Void,
        completionHandler: @escaping (Error?) -> Void
    ) -> Progress {
        let dbManager: FilesDatabaseManager
        let operation: SyncOperations.Operation
        do {
            (dbManager, operation) = try beginSyncOperation()
        } catch {
            completionHandler(error)
            return Progress()
        }

        // The protocol's completion handlers are not declared `@Sendable`, but
        // `NextcloudFileProviderKit.fetchThumbnails` (which is async-task-based) requires them
        // to be. These boxes allow forwarding the framework callbacks to the thumbnail workers.
        let perItemBox = ThumbnailingUncheckedSendable(value: perThumbnailCompletionHandler)
        let finalBox = ThumbnailingUncheckedSendable(value: completionHandler)

        let progress = thumbnailFetcher(
            itemIdentifiers,
            size,
            dbManager.account,
            ncKit,
            dbManager,
            domain,
            { id, data, error in
                if operation.isStopped {
                    perItemBox.value(id, nil, NSFileProviderError(.serverUnreachable))
                } else {
                    perItemBox.value(id, data, error)
                }
            },
            log,
            { [self] error in
                defer { operation.finish() }
                guard !retryIfStopped(operation, completion: { finalBox.value($0) }) else { return }
                finalBox.value(error)
            }
        )
        operation.cancelReadOnStop { progress.cancel() }
        return progress
    }
}

private struct ThumbnailingUncheckedSendable<Value>: @unchecked Sendable {
    let value: Value
}

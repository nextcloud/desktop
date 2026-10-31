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
        guard let ncAccount else {
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }
        guard let dbManager else {
            completionHandler(NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        // The protocol's completion handlers are not declared `@Sendable`, but
        // `NextcloudFileProviderKit.fetchThumbnails` (which is async-task-based) requires them
        // to be. These boxes allow forwarding the framework callbacks to the thumbnail workers.
        let perItemBox = ThumbnailingUncheckedSendable(value: perThumbnailCompletionHandler)
        let finalBox = ThumbnailingUncheckedSendable(value: completionHandler)

        return thumbnailFetcher(
            itemIdentifiers,
            size,
            ncAccount,
            ncKit,
            dbManager,
            domain,
            { id, data, error in perItemBox.value(id, data, error) },
            log,
            { error in finalBox.value(error) }
        )
    }
}

private struct ThumbnailingUncheckedSendable<Value>: @unchecked Sendable {
    let value: Value
}

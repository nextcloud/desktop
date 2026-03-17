//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit
import NextcloudFileProviderKit
import OSLog

extension FileProviderExtension: NSFileProviderThumbnailing {
    func fetchThumbnails(
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

        return NextcloudFileProviderKit.fetchThumbnails(
            for: itemIdentifiers,
            requestedSize: size,
            account: ncAccount,
            usingRemoteInterface: self.ncKit,
            andDatabase: dbManager,
            perThumbnailCompletionHandler: perThumbnailCompletionHandler,
            log: log,
            completionHandler: completionHandler
        )
    }
}

/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import FileProvider
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

        return NextcloudFileProviderKit.fetchThumbnails(
            for: itemIdentifiers,
            requestedSize: size,
            account: ncAccount,
            usingRemoteInterface: self.ncKit,
            perThumbnailCompletionHandler: perThumbnailCompletionHandler,
            completionHandler: completionHandler
        )
    }
}

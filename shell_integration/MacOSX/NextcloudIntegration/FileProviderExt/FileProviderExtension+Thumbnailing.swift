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
        let progress = Progress(totalUnitCount: Int64(itemIdentifiers.count))
        var progressCounter: Int64 = 0

        func finishCurrent() {
            progressCounter += 1

            if progressCounter == progress.totalUnitCount {
                completionHandler(nil)
            }
        }

        for itemIdentifier in itemIdentifiers {
            Logger.fileProviderExtension.debug(
                "Fetching thumbnail for item with identifier:\(itemIdentifier.rawValue, privacy: .public)"
            )
            guard
                let metadata = NextcloudFilesDatabaseManager.shared
                    .itemMetadataFromFileProviderItemIdentifier(itemIdentifier),
                let thumbnailUrl = metadata.thumbnailUrl(size: size)
            else {
                Logger.fileProviderExtension.debug("Did not fetch thumbnail URL")
                finishCurrent()
                continue
            }

            Logger.fileProviderExtension.debug(
                "Fetching thumbnail for file:\(metadata.fileName) at:\(thumbnailUrl.absoluteString, privacy: .public)"
            )

            ncKit.getPreview(url: thumbnailUrl) { _, data, error in
                if error == .success, data != nil {
                    perThumbnailCompletionHandler(itemIdentifier, data, nil)
                } else {
                    perThumbnailCompletionHandler(
                        itemIdentifier, nil, NSFileProviderError(.serverUnreachable))
                }
                finishCurrent()
            }
        }

        return progress
    }
}

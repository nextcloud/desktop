//
//  ThumbnailFetcher.swift
//
//
//  Created by Claudio Cambra on 15/4/24.
//

import FileProvider
import Foundation
import NextcloudKit
import OSLog

fileprivate let logger = Logger(subsystem: Logger.subsystem, category: "thumbnails")

public func fetchThumbnails(
    for itemIdentifiers: [NSFileProviderItemIdentifier],
    requestedSize size: CGSize,
    usingKit ncKit: NextcloudKit,
    perThumbnailCompletionHandler: @escaping (
        NSFileProviderItemIdentifier,
        Data?,
        Error?
    ) -> Void,
    completionHandler: @escaping (Error?) -> Void
) -> Progress {
    let progress = Progress(totalUnitCount: Int64(itemIdentifiers.count))

    func finishCurrent() {
        progress.completedUnitCount += 1

        if progress.completedUnitCount == progress.totalUnitCount {
            completionHandler(nil)
        }
    }

    for itemIdentifier in itemIdentifiers {
        // TODO: Move directly to item?
        guard let item = Item.storedItem(identifier: itemIdentifier, usingKit: ncKit),
              let thumbnailUrl = item.metadata.thumbnailUrl(size: size)
        else {
            logger.debug("Unknown thumbnail URL for: \(itemIdentifier.rawValue, privacy: .public)")
            finishCurrent()
            continue
        }

        logger.debug(
            "Fetching thumbnail for: \(item.metadata.fileName) (\(thumbnailUrl, privacy: .public))"
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

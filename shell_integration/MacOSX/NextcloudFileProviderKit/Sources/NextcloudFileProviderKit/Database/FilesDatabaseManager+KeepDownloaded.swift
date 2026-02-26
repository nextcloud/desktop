//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import RealmSwift

public extension FilesDatabaseManager {
    func set(keepDownloaded: Bool, for metadata: SendableItemMetadata) throws -> SendableItemMetadata? {
        guard let result = itemMetadatas.where({ $0.ocId == metadata.ocId }).first else {
            let error = "Did not update keepDownloaded for item metadata as it was not found."
            logger.error(error, [.item: metadata.ocId, .name: metadata.fileName])

            throw NSError(
                domain: Self.errorDomain,
                code: ErrorCode.metadataNotFound.rawValue,
                userInfo: [NSLocalizedDescriptionKey: error]
            )
        }

        try ncDatabase().write {
            result.keepDownloaded = keepDownloaded

            logger.debug("Updated keepDownloaded status for item metadata.", [.item: metadata.ocId, .name: metadata.fileName])
        }
        return SendableItemMetadata(value: result)
    }
}

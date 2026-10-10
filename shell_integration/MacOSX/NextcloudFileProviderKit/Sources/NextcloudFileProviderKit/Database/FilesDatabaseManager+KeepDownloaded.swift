//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import GRDB

public extension FilesDatabaseManager {
    func set(keepDownloaded: Bool, for metadata: SendableItemMetadata) throws -> SendableItemMetadata? {
        try write(durability: .full) { db in
            guard var record = try itemMetadata(ocId: metadata.ocId, in: db) else {
                let error = "Did not update keepDownloaded for item metadata as it was not found."
                logger.error(error, [.item: metadata.ocId, .name: metadata.fileName])

                throw NSError(
                    domain: Self.errorDomain,
                    code: ErrorCode.metadataNotFound.rawValue,
                    userInfo: [NSLocalizedDescriptionKey: error]
                )
            }

            record.keepDownloaded = keepDownloaded
            try record.upsertRow(db)

            logger.debug("Updated keepDownloaded status for item metadata.", [.item: metadata.ocId, .name: metadata.fileName])

            return record.metadata
        }
    }
}

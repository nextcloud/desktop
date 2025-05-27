//
//  SendableItemMetadata+Array.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-23.
//

import Foundation
import OSLog

extension Array<SendableItemMetadata> {
    func toFileProviderItems(
        account: Account, remoteInterface: RemoteInterface, dbManager: FilesDatabaseManager
    ) async throws -> [Item] {
        let logger = Logger(
            subsystem: Logger.subsystem, category: "itemMetadataToFileProviderItems"
        )

        return try await concurrentChunkedCompactMap { itemMetadata in
            guard !itemMetadata.e2eEncrypted else {
                logger.warning(
                    """
                    Skipping encrypted metadata in enumeration:
                        ocId: \(itemMetadata.ocId, privacy: .public)
                        fileName: \(itemMetadata.fileName, privacy: .public)
                    """
                )
                return nil
            }

            guard !isLockFileName(itemMetadata.fileName) else {
                logger.warning(
                    """
                    Skipping remote lock file item metadata in enumeration:
                        ocId: \(itemMetadata.ocId, privacy: .public)
                        fileName: \(itemMetadata.fileName, privacy: .public)
                    """
                )
                return nil
            }

            guard let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(
                itemMetadata
            ) else {
                logger.error(
                    """
                    Could not get valid parentItemIdentifier for item with ocId:
                        \(itemMetadata.ocId, privacy: .public)
                        and name: \(itemMetadata.fileName, privacy: .public)
                    """
                )
                let targetUrl = itemMetadata.serverUrl
                throw FilesDatabaseManager.parentMetadataNotFoundError(itemUrl: targetUrl)
            }
            let item = Item(
                metadata: itemMetadata,
                parentItemIdentifier: parentItemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager
            )
            logger.debug(
                """
                Will enumerate item with ocId: \(itemMetadata.ocId, privacy: .public)
                    and name: \(itemMetadata.fileName, privacy: .public)
                """
            )

            return item
        }
    }
}

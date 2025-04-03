//
//  ItemMetadata+Array.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-23.
//

import Foundation
import OSLog

extension Array<SendableItemMetadata> {
    func toFileProviderItems(
        account: Account, remoteInterface: RemoteInterface, dbManager: FilesDatabaseManager
    ) async -> [Item] {
        let logger = Logger(
            subsystem: Logger.subsystem, category: "itemMetadataToFileProviderItems"
        )

        return await concurrentChunkedCompactMap { itemMetadata in
            guard !itemMetadata.e2eEncrypted else {
                logger.error(
                    """
                    Skipping encrypted metadata in enumeration:
                    \(itemMetadata.ocId, privacy: .public)
                    \(itemMetadata.fileName, privacy: .public)
                    """
                )
                return nil
            }

            if let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(
                itemMetadata
            ) {
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
            } else {
                logger.error(
                    """
                    Could not get valid parentItemIdentifier for item with ocId:
                    \(itemMetadata.ocId, privacy: .public)
                    and name: \(itemMetadata.fileName, privacy: .public), skipping enumeration
                    """
                )
            }
            return nil
        }
    }
}

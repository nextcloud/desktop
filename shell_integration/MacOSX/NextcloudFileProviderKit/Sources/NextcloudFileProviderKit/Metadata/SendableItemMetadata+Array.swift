//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

extension [SendableItemMetadata] {
    ///
    /// Concurrently compact chunks of an array of ``SendableItemMetadata`` to an array of ``Item``.
    ///
    func toFileProviderItems(account: Account, remoteInterface: RemoteInterface, dbManager: FilesDatabaseManager, log: any FileProviderLogging) async throws -> [Item] {
        let logger = FileProviderLogger(category: "toFileProviderItems", log: log)
        let remoteSupportsTrash = await remoteInterface.supportsTrash(account: account)
        let allFilters = await Item.getContextMenuItemTypeFilters(account: account, remoteInterface: remoteInterface)

        return try await concurrentChunkedCompactMap { (itemMetadata: SendableItemMetadata) -> Item? in
            guard !itemMetadata.e2eEncrypted else {
                logger.info("Skipping encrypted metadata in enumeration.", [.item: itemMetadata.ocId, .name: itemMetadata.fileName])
                return nil
            }

            guard !isLockFileName(itemMetadata.fileName) else {
                logger.info("Skipping remote lock file item metadata in enumeration.", [.item: itemMetadata.ocId, .name: itemMetadata.fileName])
                return nil
            }

            guard let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(itemMetadata) else {
                logger.error("Could not get valid parentItemIdentifier for item by ocId.", [.item: itemMetadata.ocId, .name: itemMetadata.fileName])
                let targetUrl = itemMetadata.serverUrl
                throw FilesDatabaseManager.parentMetadataNotFoundError(itemUrl: targetUrl)
            }

            let displayFileActions = Item.typeHasApplicableContextMenuItems(filters: allFilters, candidate: itemMetadata.contentType)

            let item = Item(
                metadata: itemMetadata,
                parentItemIdentifier: parentItemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                displayFileActions: displayFileActions,
                remoteSupportsTrash: remoteSupportsTrash,
                log: log
            )

            logger.debug("Will enumerate item.", [.item: itemMetadata.ocId, .name: itemMetadata.fileName])

            return item
        }
    }
}

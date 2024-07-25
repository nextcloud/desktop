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
import OSLog

extension FilesDatabaseManager {
    public func childItemsForDirectory(_ directoryMetadata: ItemMetadata) -> [ItemMetadata] {
        var directoryServerUrl: String
        if directoryMetadata.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue {
            directoryServerUrl = directoryMetadata.serverUrl
        } else {
            directoryServerUrl = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        }
        let metadatas = ncDatabase().objects(ItemMetadata.self).filter(
            "serverUrl BEGINSWITH %@", directoryServerUrl
        )
        return sortedItemMetadatas(metadatas)
    }

    public func childDirectoriesForDirectory(_ directoryMetadata: ItemMetadata) -> [ItemMetadata] {
        let directoryServerUrl = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let metadatas = ncDatabase().objects(ItemMetadata.self).filter(
            "serverUrl BEGINSWITH %@ AND directory == true", directoryServerUrl)
        return sortedItemMetadatas(metadatas)
    }

    public func parentDirectoryMetadataForItem(_ itemMetadata: ItemMetadata) -> ItemMetadata? {
        self.itemMetadata(account: itemMetadata.account, locatedAtRemoteUrl: itemMetadata.serverUrl)
    }

    public func directoryMetadata(ocId: String) -> ItemMetadata? {
        if let metadata = ncDatabase().objects(ItemMetadata.self).filter(
            "ocId == %@ AND directory == true", ocId
        ).first {
            return ItemMetadata(value: metadata)
        }

        return nil
    }

    public func directoryMetadatas(account: String) -> [ItemMetadata] {
        let metadatas = ncDatabase().objects(ItemMetadata.self).filter(
            "account == %@ AND directory == true", account)
        return sortedItemMetadatas(metadatas)
    }

    public func directoryMetadatas(
        account: String, parentDirectoryServerUrl: String
    ) -> [ItemMetadata] {
        let metadatas = ncDatabase().objects(ItemMetadata.self).filter(
            "account == %@ AND parentDirectoryServerUrl == %@ AND directory == true", account,
            parentDirectoryServerUrl)
        return sortedItemMetadatas(metadatas)
    }

    // Deletes all metadatas related to the info of the directory provided
    public func deleteDirectoryAndSubdirectoriesMetadata(ocId: String) -> [ItemMetadata]? {
        let database = ncDatabase()
        guard
            let directoryMetadata = database.objects(ItemMetadata.self).filter(
                "ocId == %@ AND directory == true", ocId
            ).first
        else {
            Self.logger.error(
                "Could not find directory metadata for ocId \(ocId, privacy: .public). Not proceeding with deletion"
            )
            return nil
        }

        let directoryMetadataCopy = ItemMetadata(value: directoryMetadata)
        let directoryOcId = directoryMetadata.ocId
        let directoryUrlPath = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let directoryAccount = directoryMetadata.account
        let directoryEtag = directoryMetadata.etag

        Self.logger.debug(
            "Deleting root directory metadata in recursive delete. ocID: \(directoryMetadata.ocId, privacy: .public), etag: \(directoryEtag, privacy: .public), serverUrl: \(directoryUrlPath, privacy: .public)"
        )

        guard deleteItemMetadata(ocId: directoryMetadata.ocId) else {
            Self.logger.debug(
                """
                Failure to delete root directory metadata in recursive delete.
                ocID: \(directoryOcId, privacy: .public),
                etag: \(directoryEtag, privacy: .public),
                serverUrl: \(directoryUrlPath, privacy: .public)
                """
            )
            return nil
        }

        var deletedMetadatas: [ItemMetadata] = [directoryMetadataCopy]

        let results = database.objects(ItemMetadata.self).filter(
            "account == %@ AND serverUrl BEGINSWITH %@", directoryAccount, directoryUrlPath)

        for result in results {
            let resultOcId = result.ocId
            let inactiveItemMetadata = ItemMetadata(value: result)

            if deleteItemMetadata(ocId: result.ocId) {
                deletedMetadatas.append(inactiveItemMetadata)
            }

            if localFileMetadataFromOcId(resultOcId) != nil {
                deleteLocalFileMetadata(ocId: resultOcId)
            }
        }

        Self.logger.debug(
            """
            Completed deletions in directory recursive delete.
            ocID: \(directoryOcId, privacy: .public),
            etag: \(directoryEtag, privacy: .public),
            serverUrl: \(directoryUrlPath, privacy: .public)
            """
        )

        return deletedMetadatas
    }

    public func renameDirectoryAndPropagateToChildren(
        ocId: String, newServerUrl: String, newFileName: String
    ) -> [ItemMetadata]? {
        let database = ncDatabase()

        guard
            let directoryMetadata = database.objects(ItemMetadata.self).filter(
                "ocId == %@ AND directory == true", ocId
            ).first
        else {
            Self.logger.error(
                "Could not find a directory with ocID \(ocId, privacy: .public), cannot proceed with recursive renaming"
            )
            return nil
        }

        let oldItemServerUrl = directoryMetadata.serverUrl
        let oldDirectoryServerUrl = oldItemServerUrl + "/" + directoryMetadata.fileName
        let newDirectoryServerUrl = newServerUrl + "/" + newFileName
        let childItemResults = database.objects(ItemMetadata.self).filter(
            "account == %@ AND serverUrl BEGINSWITH %@", directoryMetadata.account,
            oldDirectoryServerUrl)

        renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName)
        Self.logger.debug("Renamed root renaming directory")

        do {
            try database.write {
                for childItem in childItemResults {
                    let oldServerUrl = childItem.serverUrl
                    let movedServerUrl = oldServerUrl.replacingOccurrences(
                        of: oldDirectoryServerUrl, with: newDirectoryServerUrl)
                    childItem.serverUrl = movedServerUrl
                    database.add(childItem, update: .all)
                    Self.logger.debug(
                        "Moved childItem at \(oldServerUrl) to \(movedServerUrl)")
                }
            }
        } catch {
            Self.logger.error(
                "Could not rename directory metadata with ocId: \(ocId, privacy: .public) to new serverUrl: \(newServerUrl), received error: \(error.localizedDescription, privacy: .public)"
            )

            return nil
        }

        let updatedChildItemResults = database.objects(ItemMetadata.self).filter(
            "account == %@ AND serverUrl BEGINSWITH %@", directoryMetadata.account,
            newDirectoryServerUrl)
        return sortedItemMetadatas(updatedChildItemResults)
    }
}

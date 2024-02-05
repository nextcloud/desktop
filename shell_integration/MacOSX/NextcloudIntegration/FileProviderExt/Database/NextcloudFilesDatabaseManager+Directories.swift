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

import Foundation
import OSLog

extension NextcloudFilesDatabaseManager {
    func directoryMetadata(account: String, serverUrl: String) -> NextcloudItemMetadataTable? {
        // We want to split by "/" (e.g. cloud.nc.com/files/a/b) but we need to be mindful of "https://c.nc.com"
        let problematicSeparator = "://"
        let placeholderSeparator = "__TEMP_REPLACE__"
        let serverUrlWithoutPrefix = serverUrl.replacingOccurrences(
            of: problematicSeparator, with: placeholderSeparator)
        var splitServerUrl = serverUrlWithoutPrefix.split(separator: "/")
        let directoryItemFileName = String(splitServerUrl.removeLast())
        let directoryItemServerUrl = splitServerUrl.joined(separator: "/").replacingOccurrences(
            of: placeholderSeparator, with: problematicSeparator)

        if let metadata = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "account == %@ AND serverUrl == %@ AND fileName == %@ AND directory == true", 
            account,
            directoryItemServerUrl, 
            directoryItemFileName
        ).first {
            return NextcloudItemMetadataTable(value: metadata)
        }

        return nil
    }

    func childItemsForDirectory(_ directoryMetadata: NextcloudItemMetadataTable)
        -> [NextcloudItemMetadataTable]
    {
        let directoryServerUrl = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "serverUrl BEGINSWITH %@", directoryServerUrl)
        return sortedItemMetadatas(metadatas)
    }

    func childDirectoriesForDirectory(_ directoryMetadata: NextcloudItemMetadataTable)
        -> [NextcloudItemMetadataTable]
    {
        let directoryServerUrl = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "serverUrl BEGINSWITH %@ AND directory == true", directoryServerUrl)
        return sortedItemMetadatas(metadatas)
    }

    func parentDirectoryMetadataForItem(_ itemMetadata: NextcloudItemMetadataTable)
        -> NextcloudItemMetadataTable?
    {
        directoryMetadata(account: itemMetadata.account, serverUrl: itemMetadata.serverUrl)
    }

    func directoryMetadata(ocId: String) -> NextcloudItemMetadataTable? {
        if let metadata = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "ocId == %@ AND directory == true", ocId
        ).first {
            return NextcloudItemMetadataTable(value: metadata)
        }

        return nil
    }

    func directoryMetadatas(account: String) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "account == %@ AND directory == true", account)
        return sortedItemMetadatas(metadatas)
    }

    func directoryMetadatas(account: String, parentDirectoryServerUrl: String)
        -> [NextcloudItemMetadataTable]
    {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "account == %@ AND parentDirectoryServerUrl == %@ AND directory == true", account,
            parentDirectoryServerUrl)
        return sortedItemMetadatas(metadatas)
    }

    // Deletes all metadatas related to the info of the directory provided
    func deleteDirectoryAndSubdirectoriesMetadata(ocId: String) -> [NextcloudItemMetadataTable]? {
        let database = ncDatabase()
        guard
            let directoryMetadata = database.objects(NextcloudItemMetadataTable.self).filter(
                "ocId == %@ AND directory == true", ocId
            ).first
        else {
            Logger.ncFilesDatabase.error(
                "Could not find directory metadata for ocId \(ocId, privacy: .public). Not proceeding with deletion"
            )
            return nil
        }

        let directoryMetadataCopy = NextcloudItemMetadataTable(value: directoryMetadata)
        let directoryUrlPath = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let directoryAccount = directoryMetadata.account
        let directoryEtag = directoryMetadata.etag

        Logger.ncFilesDatabase.debug(
            "Deleting root directory metadata in recursive delete. ocID: \(directoryMetadata.ocId, privacy: .public), etag: \(directoryEtag, privacy: .public), serverUrl: \(directoryUrlPath, privacy: .public)"
        )

        guard deleteItemMetadata(ocId: directoryMetadata.ocId) else {
            Logger.ncFilesDatabase.debug(
                "Failure to delete root directory metadata in recursive delete. ocID: \(directoryMetadata.ocId, privacy: .public), etag: \(directoryEtag, privacy: .public), serverUrl: \(directoryUrlPath, privacy: .public)"
            )
            return nil
        }

        var deletedMetadatas: [NextcloudItemMetadataTable] = [directoryMetadataCopy]

        let results = database.objects(NextcloudItemMetadataTable.self).filter(
            "account == %@ AND serverUrl BEGINSWITH %@", directoryAccount, directoryUrlPath)

        for result in results {
            let successfulItemMetadataDelete = deleteItemMetadata(ocId: result.ocId)
            if successfulItemMetadataDelete {
                deletedMetadatas.append(NextcloudItemMetadataTable(value: result))
            }

            if localFileMetadataFromOcId(result.ocId) != nil {
                deleteLocalFileMetadata(ocId: result.ocId)
            }
        }

        Logger.ncFilesDatabase.debug(
            "Completed deletions in directory recursive delete. ocID: \(directoryMetadata.ocId, privacy: .public), etag: \(directoryEtag, privacy: .public), serverUrl: \(directoryUrlPath, privacy: .public)"
        )

        return deletedMetadatas
    }

    func renameDirectoryAndPropagateToChildren(
        ocId: String, newServerUrl: String, newFileName: String
    ) -> [NextcloudItemMetadataTable]? {
        let database = ncDatabase()

        guard
            let directoryMetadata = database.objects(NextcloudItemMetadataTable.self).filter(
                "ocId == %@ AND directory == true", ocId
            ).first
        else {
            Logger.ncFilesDatabase.error(
                "Could not find a directory with ocID \(ocId, privacy: .public), cannot proceed with recursive renaming"
            )
            return nil
        }

        let oldItemServerUrl = directoryMetadata.serverUrl
        let oldDirectoryServerUrl = oldItemServerUrl + "/" + directoryMetadata.fileName
        let newDirectoryServerUrl = newServerUrl + "/" + newFileName
        let childItemResults = database.objects(NextcloudItemMetadataTable.self).filter(
            "account == %@ AND serverUrl BEGINSWITH %@", directoryMetadata.account,
            oldDirectoryServerUrl)

        renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName)
        Logger.ncFilesDatabase.debug("Renamed root renaming directory")

        do {
            try database.write {
                for childItem in childItemResults {
                    let oldServerUrl = childItem.serverUrl
                    let movedServerUrl = oldServerUrl.replacingOccurrences(
                        of: oldDirectoryServerUrl, with: newDirectoryServerUrl)
                    childItem.serverUrl = movedServerUrl
                    database.add(childItem, update: .all)
                    Logger.ncFilesDatabase.debug(
                        "Moved childItem at \(oldServerUrl) to \(movedServerUrl)")
                }
            }
        } catch {
            Logger.ncFilesDatabase.error(
                "Could not rename directory metadata with ocId: \(ocId, privacy: .public) to new serverUrl: \(newServerUrl), received error: \(error.localizedDescription, privacy: .public)"
            )

            return nil
        }

        let updatedChildItemResults = database.objects(NextcloudItemMetadataTable.self).filter(
            "account == %@ AND serverUrl BEGINSWITH %@", directoryMetadata.account,
            newDirectoryServerUrl)
        return sortedItemMetadatas(updatedChildItemResults)
    }
}

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
import RealmSwift

extension FilesDatabaseManager {
    func childItems(directoryMetadata: ItemMetadata) -> Results<ItemMetadata> {
        var directoryServerUrl: String
        if directoryMetadata.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue {
            directoryServerUrl = directoryMetadata.serverUrl
        } else {
            directoryServerUrl = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        }
        return itemMetadatas.filter("serverUrl BEGINSWITH %@", directoryServerUrl)
    }

    public func childItemCount(directoryMetadata: ItemMetadata) -> Int {
        childItems(directoryMetadata: directoryMetadata).count
    }

    public func parentDirectoryMetadataForItem(_ itemMetadata: ItemMetadata) -> ItemMetadata? {
        self.itemMetadata(account: itemMetadata.account, locatedAtRemoteUrl: itemMetadata.serverUrl)
    }

    public func directoryMetadata(ocId: String) -> ItemMetadata? {
        if let metadata = itemMetadatas.filter("ocId == %@ AND directory == true", ocId).first {
            return ItemMetadata(value: metadata)
        }

        return nil
    }

    public func directoryMetadatas(account: String) -> [ItemMetadata] {
        itemMetadatas
            .filter("account == %@ AND directory == true", account)
            .toUnmanagedResults()
    }

    public func directoryMetadatas(
        account: String, parentDirectoryServerUrl: String
    ) -> [ItemMetadata] {
        itemMetadatas
            .filter(
                "account == %@ AND parentDirectoryServerUrl == %@ AND directory == true",
                account,
                parentDirectoryServerUrl
            )
            .toUnmanagedResults()
    }

    // Deletes all metadatas related to the info of the directory provided
    public func deleteDirectoryAndSubdirectoriesMetadata(ocId: String) -> [ItemMetadata]? {
        let database = ncDatabase()
        guard
            let directoryMetadata = itemMetadatas.filter(
                "ocId == %@ AND directory == true", ocId
            ).first
        else {
            Self.logger.error(
                """
                Could not find directory metadata for ocId \(ocId, privacy: .public).
                    Not proceeding with deletion
                """
            )
            return nil
        }

        let directoryMetadataCopy = ItemMetadata(value: directoryMetadata)
        let directoryOcId = directoryMetadata.ocId
        let directoryUrlPath = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let directoryAccount = directoryMetadata.account
        let directoryEtag = directoryMetadata.etag

        Self.logger.debug(
            """
            Deleting root directory metadata in recursive delete.
                ocID: \(directoryMetadata.ocId, privacy: .public)
                etag: \(directoryEtag, privacy: .public)
                serverUrl: \(directoryUrlPath, privacy: .public)
            """
        )

        do {
            try database.write { database.delete(directoryMetadata) }
        } catch let error {
            Self.logger.error(
                """
                Failure to delete root directory metadata in recursive delete.
                    Received error: \(error.localizedDescription)
                    ocID: \(directoryOcId, privacy: .public),
                    etag: \(directoryEtag, privacy: .public),
                    serverUrl: \(directoryUrlPath, privacy: .public)
                """
            )
            return nil
        }

        var deletedMetadatas: [ItemMetadata] = [directoryMetadataCopy]

        let results = itemMetadatas.filter(
            "account == %@ AND serverUrl BEGINSWITH %@", directoryAccount, directoryUrlPath
        )

        for result in results {
            let inactiveItemMetadata = ItemMetadata(value: result)
            do {
                try database.write { database.delete(result) }
                deletedMetadatas.append(inactiveItemMetadata)
            } catch let error {
                Self.logger.error(
                    """
                    Failure to delete directory metadata child in recursive delete.
                        Received error: \(error.localizedDescription)
                        ocID: \(directoryOcId, privacy: .public),
                        etag: \(directoryEtag, privacy: .public),
                        serverUrl: \(directoryUrlPath, privacy: .public)
                    """
                )
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
        guard
            let directoryMetadata = itemMetadatas.filter(
                "ocId == %@ AND directory == true", ocId
            ).first
        else {
            Self.logger.error(
                """
                Could not find a directory with ocID \(ocId, privacy: .public)
                    cannot proceed with recursive renaming
                """
            )
            return nil
        }

        let oldItemServerUrl = directoryMetadata.serverUrl
        let oldDirectoryServerUrl = oldItemServerUrl + "/" + directoryMetadata.fileName
        let newDirectoryServerUrl = newServerUrl + "/" + newFileName
        let childItemResults = itemMetadatas.filter(
            "account == %@ AND serverUrl BEGINSWITH %@", directoryMetadata.account,
            oldDirectoryServerUrl)

        renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName)
        Self.logger.debug("Renamed root renaming directory")

        do {
            let database = ncDatabase()
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
                """
                Could not rename directory metadata with ocId: \(ocId, privacy: .public)
                    to new serverUrl: \(newServerUrl)
                    received error: \(error.localizedDescription, privacy: .public)
                """
            )

            return nil
        }

        return itemMetadatas
            .filter(
                "account == %@ AND serverUrl BEGINSWITH %@",
                directoryMetadata.account,
                newDirectoryServerUrl
            )
            .toUnmanagedResults()
    }
}

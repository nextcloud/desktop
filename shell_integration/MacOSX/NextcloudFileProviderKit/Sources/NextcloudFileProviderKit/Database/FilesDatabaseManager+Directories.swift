//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import RealmSwift

public extension FilesDatabaseManager {
    private func fullServerPathUrl(for metadata: any ItemMetadata) -> String {
        if metadata.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue {
            metadata.serverUrl
        } else {
            metadata.serverUrl + "/" + metadata.fileName
        }
    }

    func childItems(directoryMetadata: SendableItemMetadata) -> [SendableItemMetadata] {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)
        return itemMetadatas
            .where { $0.serverUrl.starts(with: directoryServerUrl) }
            .toUnmanagedResults()
    }

    func childItemCount(directoryMetadata: SendableItemMetadata) -> Int {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)
        return itemMetadatas
            .where { $0.serverUrl.starts(with: directoryServerUrl) }
            .count
    }

    func parentDirectoryMetadataForItem(_ itemMetadata: SendableItemMetadata) -> SendableItemMetadata? {
        self.itemMetadata(account: itemMetadata.account, locatedAtRemoteUrl: itemMetadata.serverUrl)
    }

    func directoryMetadata(ocId: String) -> SendableItemMetadata? {
        if let metadata = itemMetadatas.where({ $0.ocId == ocId && $0.directory }).first {
            return SendableItemMetadata(value: metadata)
        }

        return nil
    }

    /// Deletes all metadatas related to the info of the directory provided
    func deleteDirectoryAndSubdirectoriesMetadata(
        ocId: String
    ) -> [SendableItemMetadata]? {
        guard let directoryMetadata = itemMetadatas
            .where({ $0.ocId == ocId && $0.directory })
            .first
        else {
            logger.error("Could not find directory metadata for ocId. Not proceeding with deletion.", [.item: ocId])
            return nil
        }

        let directoryMetadataCopy = SendableItemMetadata(value: directoryMetadata)
        let directoryOcId = directoryMetadata.ocId
        let directoryUrlPath = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let directoryAccount = directoryMetadata.account
        let directoryEtag = directoryMetadata.etag

        logger.debug("Deleting root directory metadata in recursive delete.", [.eTag: directoryEtag, .item: directoryMetadata.ocId, .url: directoryUrlPath])

        let database = ncDatabase()
        do {
            try database.write { directoryMetadata.deleted = true }
        } catch {
            logger.error("Failure to delete root directory metadata in recursive delete.", [.error: error, .eTag: directoryEtag, .item: directoryOcId, .url: directoryUrlPath])
            return nil
        }

        var deletedMetadatas: [SendableItemMetadata] = [directoryMetadataCopy]

        let results = itemMetadatas.where {
            $0.account == directoryAccount && $0.serverUrl.starts(with: directoryUrlPath)
        }

        for result in results {
            let inactiveItemMetadata = SendableItemMetadata(value: result)
            do {
                try database.write { result.deleted = true }
                deletedMetadatas.append(inactiveItemMetadata)
            } catch {
                logger.error("Failure to delete directory metadata child in recursive delete", [.error: error, .eTag: directoryEtag, .item: directoryOcId, .url: directoryUrlPath])
            }
        }

        logger.debug("Completed deletions in directory recursive delete.", [.eTag: directoryEtag, .item: directoryOcId, .url: directoryUrlPath])

        return deletedMetadatas
    }

    func renameDirectoryAndPropagateToChildren(
        ocId: String, newServerUrl: String, newFileName: String
    ) -> [SendableItemMetadata]? {
        guard let directoryMetadata = itemMetadatas
            .where({ $0.ocId == ocId && $0.directory })
            .first
        else {
            logger.error("Could not find a directory with ocID \(ocId), cannot proceed with recursive renaming.", [.item: ocId])
            return nil
        }

        let oldItemServerUrl = directoryMetadata.serverUrl
        let oldItemFilename = directoryMetadata.fileName
        let oldDirectoryServerUrl = oldItemServerUrl + "/" + oldItemFilename
        let newDirectoryServerUrl = newServerUrl + "/" + newFileName
        let childItemResults = itemMetadatas.where {
            $0.account == directoryMetadata.account &&
                $0.serverUrl.starts(with: oldDirectoryServerUrl)
        }

        renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName)
        logger.debug("Renamed root renaming directory from \"\(oldDirectoryServerUrl)\" to \"\(newDirectoryServerUrl)\".", [.item: ocId])

        do {
            let database = ncDatabase()
            try database.write {
                for childItem in childItemResults {
                    let oldServerUrl = childItem.serverUrl
                    let movedServerUrl = oldServerUrl.replacingOccurrences(
                        of: oldDirectoryServerUrl, with: newDirectoryServerUrl
                    )
                    childItem.serverUrl = movedServerUrl
                    database.add(childItem, update: .all)
                    logger.debug(
                        """
                        Moved childItem at: \(oldServerUrl)
                                        to: \(movedServerUrl)
                        """
                    )
                }
            }
        } catch {
            logger.error("Could not rename directory metadata.", [.error: error, .item: ocId, .url: newServerUrl])
            return nil
        }

        return itemMetadatas
            .where {
                $0.account == directoryMetadata.account &&
                    $0.serverUrl.starts(with: newDirectoryServerUrl)
            }
            .toUnmanagedResults()
    }
}

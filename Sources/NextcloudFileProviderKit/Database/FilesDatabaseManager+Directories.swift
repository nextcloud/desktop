//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import OSLog
import RealmSwift

extension FilesDatabaseManager {
    private func fullServerPathUrl(for metadata: any ItemMetadata) -> String {
        if metadata.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue {
            metadata.serverUrl
        } else {
            metadata.serverUrl + "/" + metadata.fileName
        }
    }

    public func childItems(directoryMetadata: SendableItemMetadata) -> [SendableItemMetadata] {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)
        return itemMetadatas
            .where({ $0.serverUrl.starts(with: directoryServerUrl) })
            .toUnmanagedResults()
    }

    public func childItemCount(directoryMetadata: SendableItemMetadata) -> Int {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)
        return itemMetadatas
            .where({ $0.serverUrl.starts(with: directoryServerUrl) })
            .count
    }

    public func parentDirectoryMetadataForItem(
        _ itemMetadata: SendableItemMetadata
    ) -> SendableItemMetadata? {
        self.itemMetadata(
            account: itemMetadata.account, locatedAtRemoteUrl: itemMetadata.serverUrl
        )
    }

    public func directoryMetadata(ocId: String) -> SendableItemMetadata? {
        if let metadata = itemMetadatas.where({ $0.ocId == ocId && $0.directory }).first {
            return SendableItemMetadata(value: metadata)
        }

        return nil
    }

    // Deletes all metadatas related to the info of the directory provided
    public func deleteDirectoryAndSubdirectoriesMetadata(
        ocId: String
    ) -> [SendableItemMetadata]? {
        guard let directoryMetadata = itemMetadatas
            .where({ $0.ocId == ocId && $0.directory })
            .first
        else {
            Self.logger.error(
                """
                Could not find directory metadata for ocId \(ocId, privacy: .public).
                    Not proceeding with deletion
                """
            )
            return nil
        }

        let directoryMetadataCopy = SendableItemMetadata(value: directoryMetadata)
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

        let database = ncDatabase()
        do {
            try database.write { directoryMetadata.deleted = true }
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

        var deletedMetadatas: [SendableItemMetadata] = [directoryMetadataCopy]

        let results = itemMetadatas.where {
            $0.account == directoryAccount && $0.serverUrl.starts(with: directoryUrlPath)
        }

        for result in results {
            let inactiveItemMetadata = SendableItemMetadata(value: result)
            do {
                try database.write { result.deleted = true }
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
    ) -> [SendableItemMetadata]? {
        guard let directoryMetadata = itemMetadatas
            .where({ $0.ocId == ocId && $0.directory })
            .first
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
        let oldItemFilename = directoryMetadata.fileName
        let oldDirectoryServerUrl = oldItemServerUrl + "/" + oldItemFilename
        let newDirectoryServerUrl = newServerUrl + "/" + newFileName
        let childItemResults = itemMetadatas.where {
            $0.account == directoryMetadata.account &&
            $0.serverUrl.starts(with: oldDirectoryServerUrl)
        }

        renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName)
        Self.logger.debug(
            """
            Renamed root renaming directory from: \(oldDirectoryServerUrl, privacy: .public)
                                              to: \(newDirectoryServerUrl, privacy: .public)
            """
        )

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
                        """
                        Moved childItem at: \(oldServerUrl, privacy: .public)
                                        to: \(movedServerUrl, privacy: .public)
                        """)
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
            .where {
                $0.account == directoryMetadata.account &&
                $0.serverUrl.starts(with: newDirectoryServerUrl)
            }
            .toUnmanagedResults()
    }
}

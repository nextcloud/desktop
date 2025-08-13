//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit

public extension Item {
    // NOTE: The trashing parameter does not affect whether the server will trash this or not.
    // That's out of our hands. Instead, this is used internally to properly handle the metadata
    // update automatically when we conduct a move of an item to the trash.
    func delete(
        trashing: Bool = false,
        options: NSFileProviderDeleteItemOptions = [.recursive],
        domain: NSFileProviderDomain? = nil,
        ignoredFiles: IgnoredFilesMatcher? = nil,
        dbManager: FilesDatabaseManager
    ) async -> Error? {
        let isEmptyDirOrIsFile = childItemCount == nil || childItemCount == 0
        guard trashing || isEmptyDirOrIsFile || options.contains(.recursive) else {
            return NSFileProviderError(.directoryNotEmpty)
        }

        let ocId = itemIdentifier.rawValue
        let relativePath = (
            metadata.serverUrl + "/" + metadata.fileName
        ).replacingOccurrences(of: metadata.urlBase, with: "")

        guard ignoredFiles == nil || ignoredFiles?.isExcluded(relativePath) == false else {
            logger.info(
                """
                File \(self.filename) is in the ignore list.
                    Will delete locally with no remote effect.
                """
            )
            dbManager.deleteItemMetadata(ocId: ocId)
            return nil
        }

        let serverFileNameUrl = metadata.serverUrl + "/" + metadata.fileName
        guard serverFileNameUrl != "" else {
            return NSError.fileProviderErrorForNonExistentItem(withIdentifier: self.itemIdentifier)
        }

        guard metadata.classFile != "lock", !isLockFileName(metadata.fileName) else {
            return await deleteLockFile(domain: domain, dbManager: dbManager)
        }

        let (_, _, error) = await remoteInterface.delete(
            remotePath: serverFileNameUrl,
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
        })

        guard error == .success else {
            logger.error(
                """
                Could not delete item with ocId \(ocId)...
                at \(serverFileNameUrl)...
                received error: \(error.errorCode)
                \(error.errorDescription)
                """
            )
            return error.fileProviderError(
                handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
            )
        }

        logger.info(
            """
            Successfully deleted item with identifier: \(ocId)...
            at: \(serverFileNameUrl)
            """
        )

        guard trashing else {
            handleMetadataDeletion()
            return nil
        }
        return handleMetadataTrashModification()
    }

    private func handleMetadataDeletion() {
        let ocId = metadata.ocId
        if self.metadata.directory {
            _ = dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: ocId)
        } else {
            dbManager.deleteItemMetadata(ocId: ocId)
        }
    }

    // NOTE: the trashing metadata modification procedure here is rough. You SHOULD run a rescan of
    // the trash in order to ensure you are getting a correct picture of the item's current remote
    // state! This is important particularly for receiving the correct trash bin filename in case of
    // there being a previous item in the trash with the same name, prompting the server to rename
    // the newly-trashed target item
    private func handleMetadataTrashModification() -> Error? {
        let ocId = metadata.ocId
        if metadata.directory {
            _ = dbManager.renameDirectoryAndPropagateToChildren(
                ocId: ocId,
                newServerUrl: account.trashUrl,
                newFileName: filename
            )
        } else {
            dbManager.renameItemMetadata(
                ocId: ocId, newServerUrl: account.trashUrl, newFileName: filename
            )
        }

        guard var metadata = dbManager.itemMetadata(ocId: ocId) else {
            logger.info(
                """
                Could not find item metadata for \(self.filename)
                    \(self.itemIdentifier.rawValue)!
                    Cannot finish trashing procedure.
                """
            )
            return NSFileProviderError(.cannotSynchronize)
        }
        metadata.trashbinFileName = filename
        metadata.trashbinDeletionTime = Date()
        metadata.trashbinOriginalLocation =
            String(self.metadata.serverUrl + "/" + filename).replacingOccurrences(
                of: account.davFilesUrl + "/", with: ""
            )

        dbManager.addItemMetadata(metadata)
        return nil
    }
}

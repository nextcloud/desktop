//
//  Item+Delete.swift
//
//
//  Created by Claudio Cambra on 15/4/24.
//

import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit
import OSLog

public extension Item {
    // NOTE: the trashing metadata modification procedure here is rough. You SHOULD run a rescan of
    // the trash in order to ensure you are getting a correct picture of the item's current remote
    // state! This is important particularly for receiving the correct trash bin filename in case of
    // there being a previous item in the trash with the same name, prompting the server to rename
    // the newly-trashed target item
    func delete(
        trashing: Bool = false,
        domain: NSFileProviderDomain? = nil,
        ignoredFiles: IgnoredFilesMatcher? = nil,
        dbManager: FilesDatabaseManager
    ) async -> Error? {
        let ocId = itemIdentifier.rawValue
        let relativePath = (
            metadata.serverUrl + "/" + metadata.fileName
        ).replacingOccurrences(of: metadata.urlBase, with: "")

        guard ignoredFiles == nil || ignoredFiles?.isExcluded(relativePath) == false else {
            Self.logger.info(
                """
                File \(self.filename, privacy: .public) is in the ignore list.
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
            Self.logger.error(
                """
                Could not delete item with ocId \(ocId, privacy: .public)...
                at \(serverFileNameUrl, privacy: .public)...
                received error: \(error.errorCode, privacy: .public)
                \(error.errorDescription, privacy: .public)
                """
            )
            return error.fileProviderError(
                handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
            )
        }

        Self.logger.info(
            """
            Successfully deleted item with identifier: \(ocId, privacy: .public)...
            at: \(serverFileNameUrl, privacy: .public)
            """
        )

        guard !trashing else {
            if self.metadata.directory {
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
                Self.logger.warning(
                    """
                    Could not find item metadata for \(self.filename, privacy: .public)
                    \(self.itemIdentifier.rawValue, privacy: .public)!
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
        if self.metadata.directory {
            _ = dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: ocId)
        } else {
            dbManager.deleteItemMetadata(ocId: ocId)
        }
        return nil
    }
}

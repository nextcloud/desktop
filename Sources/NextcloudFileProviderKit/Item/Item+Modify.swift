//
//  Item+Modify.swift
//  
//
//  Created by Claudio Cambra on 16/4/24.
//

import FileProvider
import Foundation
import NextcloudKit
import OSLog

public extension Item {

    func move(
        newFileName: String,
        newRemotePath: String,
        newParentItemIdentifier: NSFileProviderItemIdentifier,
        newParentItemRemotePath: String,
        domain: NSFileProviderDomain? = nil,
        dbManager: FilesDatabaseManager = .shared
    ) async -> (Item?, Error?) {
        let ocId = itemIdentifier.rawValue
        let isFolder = contentType == .folder || contentType == .directory || contentType == .bundle
        let oldRemotePath = metadata.serverUrl + "/" + metadata.fileName
        let (_, moveError) = await remoteInterface.move(
            remotePathSource: oldRemotePath,
            remotePathDestination: newRemotePath,
            overwrite: false,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard moveError == .success else {
            Self.logger.error(
                """
                Could not move file or folder: \(oldRemotePath, privacy: .public)
                to \(newRemotePath, privacy: .public),
                received error: \(moveError.errorCode, privacy: .public)
                \(moveError.errorDescription, privacy: .public)
                """
            )
            return (
                nil,
                moveError.matchesCollisionError ?
                    NSFileProviderError(.filenameCollision) : moveError.fileProviderError
            )
        }

        // Remember that a folder metadata's serverUrl is its direct server URL, while for
        // an item metadata the server URL is the parent folder's URL
        if isFolder {
            _ = dbManager.renameDirectoryAndPropagateToChildren(
                ocId: ocId,
                newServerUrl: newRemotePath,
                newFileName: newFileName
            )
        } else {
            dbManager.renameItemMetadata(
                ocId: ocId,
                newServerUrl: newParentItemRemotePath,
                newFileName: newFileName
            )
        }

        guard let newMetadata = dbManager.itemMetadataFromOcId(ocId) else {
            Self.logger.error(
                """
                Could not acquire metadata of item with identifier: \(ocId, privacy: .public),
                cannot correctly inform of modification
                """
            )
            return (nil, NSFileProviderError(.noSuchItem))
        }

        let modifiedItem = Item(
            metadata: newMetadata,
            parentItemIdentifier: newParentItemIdentifier,
            remoteInterface: remoteInterface
        )
        return (modifiedItem, nil)
    }

    private func modifyContents(
        contents newContents: URL?,
        remotePath: String,
        newCreationDate: Date?,
        newContentModificationDate: Date?,
        domain: NSFileProviderDomain?,
        progress: Progress,
        dbManager: FilesDatabaseManager
    ) async -> (Item?, Error?) {
        let ocId = itemIdentifier.rawValue

        guard newContents != nil, let localPath = newContents?.path else {
            Self.logger.error(
                """
                ERROR. Could not upload modified contents as was provided nil contents url.
                ocId: \(ocId, privacy: .public) filename: \(self.filename, privacy: .public)
                """
            )
            return (nil, NSFileProviderError(.noSuchItem))
        }

        guard let metadata = dbManager.itemMetadataFromOcId(ocId) else {
            Self.logger.error(
                "Could not acquire metadata of item with identifier: \(ocId, privacy: .public)"
            )
            return (nil, NSFileProviderError(.noSuchItem))
        }

        let updatedMetadata = await withCheckedContinuation { continuation in
            dbManager.setStatusForItemMetadata(
                metadata, status: ItemMetadata.Status.uploading
            ) { continuation.resume(returning: $0) }
        }

        if updatedMetadata == nil {
            Self.logger.warning(
                """
                Could not acquire updated metadata of item: \(ocId, privacy: .public),
                unable to update item status to uploading
                """
            )
        }

        let (_, _, etag, date, size, _, _, error) = await remoteInterface.upload(
            remotePath: remotePath,
            localPath: localPath,
            creationDate: newCreationDate,
            modificationDate: newContentModificationDate,
            options: .init(),
            requestHandler: { progress.setHandlersFromAfRequest($0) },
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            },
            progressHandler: { $0.copyCurrentStateToProgress(progress) }
        )

        guard error == .success else {
            Self.logger.error(
                """
                Could not upload item \(ocId, privacy: .public)
                with filename: \(self.filename, privacy: .public),
                received error: \(error.errorCode, privacy: .public),
                \(error.errorDescription, privacy: .public)
                """
            )

            metadata.status = ItemMetadata.Status.uploadError.rawValue
            metadata.sessionError = error.errorDescription
            dbManager.addItemMetadata(metadata)
            return (nil, error.fileProviderError)
        }

        Self.logger.info(
            """
            Successfully uploaded item with identifier: \(ocId, privacy: .public)
            and filename: \(self.filename, privacy: .public)
            """
        )

        if size != documentSize as? Int64 {
            Self.logger.warning(
                """
                Item content modification upload reported as successful,
                but there are differences between the received file size (\(size, privacy: .public))
                and the original file size (\(self.documentSize?.int64Value ?? 0))
                """
            )
        }

        let newMetadata = ItemMetadata()
        newMetadata.date = (date ?? NSDate()) as Date
        newMetadata.etag = etag ?? metadata.etag
        newMetadata.account = metadata.account
        newMetadata.fileName = metadata.fileName
        newMetadata.fileNameView = metadata.fileNameView
        newMetadata.ocId = ocId
        newMetadata.size = size
        newMetadata.contentType = metadata.contentType
        newMetadata.directory = metadata.directory
        newMetadata.serverUrl = metadata.serverUrl
        newMetadata.session = ""
        newMetadata.sessionError = ""
        newMetadata.sessionTaskIdentifier = 0
        newMetadata.status = ItemMetadata.Status.normal.rawValue

        dbManager.addLocalFileMetadataFromItemMetadata(newMetadata)
        dbManager.addItemMetadata(newMetadata)

        let modifiedItem = Item(
            metadata: newMetadata,
            parentItemIdentifier: parentItemIdentifier,
            remoteInterface: remoteInterface
        )
        return (modifiedItem, nil)
    }

    func modify(
        itemTarget: NSFileProviderItem,
        baseVersion: NSFileProviderItemVersion = NSFileProviderItemVersion(),
        changedFields: NSFileProviderItemFields,
        contents newContents: URL?,
        options: NSFileProviderModifyItemOptions = [],
        request: NSFileProviderRequest = NSFileProviderRequest(),
        ncAccount: Account,
        domain: NSFileProviderDomain? = nil,
        progress: Progress = .init(),
        dbManager: FilesDatabaseManager = .shared
    ) async -> (Item?, Error?) {
        let ocId = itemIdentifier.rawValue
        guard itemTarget.itemIdentifier == itemIdentifier else {
            Self.logger.error(
                """
                Could not modify item: \(ocId, privacy: .public), different identifier to the
                item the modification was targeting
                (\(itemTarget.itemIdentifier.rawValue, privacy: .public))
                """
            )
            return (nil, NSFileProviderError(.noSuchItem))
        }

        let parentItemIdentifier = itemTarget.parentItemIdentifier
        let isFolder = contentType == .folder || contentType == .directory || contentType == .bundle

        if options.contains(.mayAlreadyExist) {
            // TODO: This needs to be properly handled with a check in the db
            Self.logger.warning(
                "Modification for item: \(ocId, privacy: .public) may already exist"
            )
        }

        var parentItemServerUrl: String

        // The target parent should already be present in our database. The system will have synced
        // remote changes and then, upon user interaction, will try to modify the item.
        // That is, if the parent item has changed at all (it might not have)
        if parentItemIdentifier == .rootContainer {
            parentItemServerUrl = ncAccount.davFilesUrl
        } else {
            guard let parentItemMetadata = dbManager.directoryMetadata(
                ocId: parentItemIdentifier.rawValue
            ) else {
                Self.logger.error(
                    """
                    Not modifying item: \(ocId, privacy: .public),
                    could not find metadata for target parentItemIdentifier
                        \(parentItemIdentifier.rawValue, privacy: .public)
                    """
                )
                return (nil, NSFileProviderError(.noSuchItem))
            }

            parentItemServerUrl = parentItemMetadata.serverUrl + "/" + parentItemMetadata.fileName
        }

        let newServerUrlFileName = parentItemServerUrl + "/" + itemTarget.filename

        Self.logger.debug(
            """
            About to modify item with identifier: \(ocId, privacy: .public)
            of type: \(self.contentType.identifier)
            (is folder: \(isFolder ? "yes" : "no", privacy: .public)
            and filename: \(itemTarget.filename, privacy: .public)
            to server url: \(newServerUrlFileName, privacy: .public)
            with contents located at: \(newContents?.path ?? "", privacy: .public)
            """
        )

        var modifiedItem = self

        if changedFields.contains(.filename) || changedFields.contains(.parentItemIdentifier) {
            Self.logger.debug(
                """
                Changed fields for item \(ocId, privacy: .public)
                with filename \(self.filename, privacy: .public)
                includes filename or parentitemidentifier.
                old filename: \(self.filename, privacy: .public)
                new filename: \(itemTarget.filename, privacy: .public)
                old parent identifier: \(parentItemIdentifier.rawValue, privacy: .public)
                new parent identifier: \(itemTarget.parentItemIdentifier.rawValue, privacy: .public)
                """
            )

            let (renameModifiedItem, renameError) = await modifiedItem.move(
                newFileName: itemTarget.filename,
                newRemotePath: newServerUrlFileName,
                newParentItemIdentifier: parentItemIdentifier,
                newParentItemRemotePath: parentItemServerUrl,
                dbManager: dbManager
            )

            guard renameError == nil, let renameModifiedItem else {
                Self.logger.error(
                    """
                    Could not rename item with ocID \(ocId, privacy: .public)
                    (\(self.filename, privacy: .public)) to
                    \(newServerUrlFileName, privacy: .public),
                    received error: \(renameError?.localizedDescription ?? "", privacy: .public)
                    """
                )
                return (nil, renameError)
            }

            modifiedItem = renameModifiedItem

            guard !isFolder else {
                Self.logger.debug(
                    """
                    Rename of folder \(ocId, privacy: .public) (\(self.filename, privacy: .public))
                    to \(itemTarget.filename, privacy: .public)
                    at \(newServerUrlFileName, privacy: .public)
                    complete. Only handling renaming for folder and no other procedures.
                    """
                )
                return (modifiedItem, nil)
            }
        }

        guard !isFolder else {
            Self.logger.debug(
                """
                System requested modification for folder with ocID \(ocId, privacy: .public)
                (\(newServerUrlFileName, privacy: .public)) of something other than folder name.
                This is not supported.
                """
            )
            return (modifiedItem, nil)
        }

        if changedFields.contains(.contents) {
            Self.logger.debug(
                """
                Item modification for \(ocId, privacy: .public)
                \(modifiedItem.filename, privacy: .public)
                includes contents. Will begin upload.
                """
            )

            let newCreationDate = itemTarget.creationDate ?? creationDate
            let newContentModificationDate = 
                itemTarget.contentModificationDate ?? contentModificationDate
            let (contentModifiedItem, contentError) = await modifiedItem.modifyContents(
                contents: newContents,
                remotePath: newServerUrlFileName,
                newCreationDate: newCreationDate,
                newContentModificationDate: newContentModificationDate,
                domain: domain,
                progress: progress,
                dbManager: dbManager
            )

            guard contentError == nil, let contentModifiedItem else {
                Self.logger.error(
                    """
                    Could not modify contents for item with ocID \(ocId, privacy: .public)
                    (\(modifiedItem.filename, privacy: .public)) to
                    \(newServerUrlFileName, privacy: .public),
                    received error: \(contentError?.localizedDescription ?? "", privacy: .public)
                    """
                )
                return (nil, contentError)
            }

            modifiedItem = contentModifiedItem
        }

        Self.logger.debug(
            """
            Nothing more to do with \(ocId, privacy: .public)
            \(modifiedItem.filename, privacy: .public), modifications complete
            """
        )
        return (modifiedItem, nil)
    }
}

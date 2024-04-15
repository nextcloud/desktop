//
//  Item+Operations.swift
//
//
//  Created by Claudio Cambra on 15/4/24.
//

import FileProvider
import Foundation
import NextcloudKit
import OSLog

extension Item {

    private static func createNewFolder(
        remotePath: String,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        ncKit: NextcloudKit,
        progress: Progress
    ) async -> (Item?, Error?) {
        let (account, createError) = await withCheckedContinuation { continuation in
            ncKit.createFolder(serverUrlFileName: remotePath) { account, _, _, error in
                continuation.resume(returning: (account, error.fileProviderError))
            }
        }

        guard createError == nil else {
            Self.logger.error(
                """
                Could not create new folder at: \(remotePath, privacy: .public),
                received error: \(createError?.localizedDescription ?? "", privacy: .public)
                """
            )
            return (nil, createError)
        }

        // Read contents after creation
        let (files, readError) = await withCheckedContinuation { continuation in
            ncKit.readFileOrFolder(
                serverUrlFileName: remotePath, depth: "0", showHiddenFiles: true
            ) { account, files, _, error in
                continuation.resume(returning: (files, error.fileProviderError))
            }
        }

        guard readError == nil else {
            Self.logger.error(
                """
                Could not read new folder at: \(remotePath, privacy: .public),
                received error: \(readError?.localizedDescription ?? "", privacy: .public)
                """
            )
            return (nil, readError)
        }

        let directoryMetadata = await withCheckedContinuation { continuation in
            ItemMetadata.metadatasFromDirectoryReadNKFiles(
                files, account: account
            ) { directoryMetadata, _, _ in
                continuation.resume(returning: directoryMetadata)
            }
        }

        FilesDatabaseManager.shared.addItemMetadata(directoryMetadata)

        let fpItem = Item(
            metadata: directoryMetadata,
            parentItemIdentifier: parentItemIdentifier,
            ncKit: ncKit
        )

        return (fpItem, nil)
    }

    private static func createNewFile(
        remotePath: String,
        localPath: String,
        itemTemplate: NSFileProviderItem,
        parentItemRemotePath: String,
        domain: NSFileProviderDomain? = nil,
        ncKit: NextcloudKit,
        progress: Progress
    ) async -> (Item?, Error?) {
        let (account, ocId, etag, date, size, error) = await withCheckedContinuation { 
            continuation in
            
            ncKit.upload(
                serverUrlFileName: remotePath,
                fileNameLocalPath: localPath,
                requestHandler: { request in
                    progress.setHandlersFromAfRequest(request)
                },
                taskHandler: { task in
                    if let domain = domain {
                        NSFileProviderManager(for: domain)?.register(
                            task,
                            forItemWithIdentifier: itemTemplate.itemIdentifier,
                            completionHandler: { _ in }
                        )
                    }
                },
                progressHandler: { uploadProgress in
                    uploadProgress.copyCurrentStateToProgress(progress)
                }
            ) { account, ocId, etag, date, size, _, _, error in
                continuation.resume(
                    returning: (account, ocId, etag, date, size, error.fileProviderError)
                )
            }
        }

        guard error == nil, let ocId = ocId else {
            Self.logger.error(
                """
                Could not upload item with filename: \(itemTemplate.filename, privacy: .public),
                received error: \(error?.localizedDescription ?? "", privacy: .public)
                received ocId: \(ocId ?? "empty", privacy: .public)
                """
            )
            return (nil, error)
        }

        Self.logger.info(
            """
            Successfully uploaded item with identifier: \(ocId, privacy: .public)
            and filename: \(itemTemplate.filename, privacy: .public)
            """
        )

        if size != itemTemplate.documentSize as? Int64 {
            Self.logger.warning(
                """
                Created item upload reported as successful, but there are differences between
                the received file size (\(size, privacy: .public))
                and the original file size (\(itemTemplate.documentSize??.int64Value ?? 0))
                """
            )
        }

        let newMetadata = ItemMetadata()
        newMetadata.date = (date ?? NSDate()) as Date
        newMetadata.etag = etag ?? ""
        newMetadata.account = account
        newMetadata.fileName = itemTemplate.filename
        newMetadata.fileNameView = itemTemplate.filename
        newMetadata.ocId = ocId
        newMetadata.size = size
        newMetadata.contentType = itemTemplate.contentType?.preferredMIMEType ?? ""
        newMetadata.directory = false
        newMetadata.serverUrl = parentItemRemotePath
        newMetadata.session = ""
        newMetadata.sessionError = ""
        newMetadata.sessionTaskIdentifier = 0
        newMetadata.status = ItemMetadata.Status.normal.rawValue

        let dbManager = FilesDatabaseManager.shared
        dbManager.addLocalFileMetadataFromItemMetadata(newMetadata)
        dbManager.addItemMetadata(newMetadata)

        let fpItem = Item(
            metadata: newMetadata,
            parentItemIdentifier: itemTemplate.parentItemIdentifier,
            ncKit: ncKit
        )

        return (fpItem, nil)
    }

    public static func create(
        basedOn itemTemplate: NSFileProviderItem,
        fields: NSFileProviderItemFields = NSFileProviderItemFields(),
        contents url: URL?,
        options: NSFileProviderCreateItemOptions = [],
        request: NSFileProviderRequest = NSFileProviderRequest(),
        domain: NSFileProviderDomain? = nil,
        ncKit: NextcloudKit,
        ncAccount: Account,
        progress: Progress
    ) async -> (Item?, Error?) {
        let tempId = itemTemplate.itemIdentifier.rawValue

        guard itemTemplate.contentType != .symbolicLink else {
            Self.logger.error(
                "Cannot create item \(tempId, privacy: .public), symbolic links not supported."
            )
            return (nil, NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError))
        }

        if options.contains(.mayAlreadyExist) {
            // TODO: This needs to be properly handled with a check in the db
            Self.logger.info(
                """
                Not creating item: \(itemTemplate.itemIdentifier.rawValue, privacy: .public)
                as it may already exist
                """
            )
            return (nil, NSFileProviderError(.noSuchItem))
        }

        let parentItemIdentifier = itemTemplate.parentItemIdentifier
        var parentItemRemotePath: String

        if parentItemIdentifier == .rootContainer {
            parentItemRemotePath = ncAccount.davFilesUrl
        } else {
            guard let parentItemMetadata = FilesDatabaseManager.shared.directoryMetadata(
                ocId: parentItemIdentifier.rawValue
            ) else {
                Self.logger.error(
                    """
                    Not creating item: \(itemTemplate.itemIdentifier.rawValue, privacy: .public),
                    could not find metadata for parentItemIdentifier:
                        \(parentItemIdentifier.rawValue, privacy: .public)
                    """
                )
                return (nil, NSFileProviderError(.noSuchItem))
            }
            parentItemRemotePath = parentItemMetadata.serverUrl + "/" + parentItemMetadata.fileName
        }

        let fileNameLocalPath = url?.path ?? ""
        let newServerUrlFileName = parentItemRemotePath + "/" + itemTemplate.filename
        let itemTemplateIsFolder =
            itemTemplate.contentType == .folder || itemTemplate.contentType == .directory

        Self.logger.debug(
            """
            About to upload item with identifier: \(tempId, privacy: .public)
            of type: \(itemTemplate.contentType?.identifier ?? "UNKNOWN", privacy: .public)
            (is folder: \(itemTemplateIsFolder ? "yes" : "no", privacy: .public)
            and filename: \(itemTemplate.filename, privacy: .public)
            to server url: \(newServerUrlFileName, privacy: .public)
            with contents located at: \(fileNameLocalPath, privacy: .public)
            """
        )

        guard !itemTemplateIsFolder else  {
            return await Self.createNewFolder(
                remotePath: newServerUrlFileName,
                parentItemIdentifier: parentItemIdentifier,
                ncKit: ncKit,
                progress: progress
            )
        }


        return await Self.createNewFile(
            remotePath: newServerUrlFileName,
            localPath: fileNameLocalPath,
            itemTemplate: itemTemplate,
            parentItemRemotePath: parentItemRemotePath,
            domain: domain,
            ncKit: ncKit,
            progress: progress
        )
    }

    public func fetchContents(
        domain: NSFileProviderDomain, progress: Progress
    ) async -> (URL?, Item?, Error?) {
        let ocId = itemIdentifier.rawValue
        let serverUrlFileName = metadata.serverUrl + "/" + metadata.fileName

        Self.logger.debug(
            """
            Fetching file with name \(self.metadata.fileName, privacy: .public)
            at URL: \(serverUrlFileName, privacy: .public)
            """
        )

        // TODO: Handle folders nicely
        var fileNameLocalPath: URL?
        do {
            fileNameLocalPath = try localPathForNCFile(
                ocId: metadata.ocId, fileNameView: metadata.fileNameView, domain: domain
            )
        } catch {
            Self.logger.error(
                """
                Could not find local path for file \(self.metadata.fileName, privacy: .public),
                received error: \(error.localizedDescription, privacy: .public)
                """
            )
            return (nil, nil, NSFileProviderError(.cannotSynchronize))
        }

        guard let fileNameLocalPath = fileNameLocalPath else {
            Self.logger.error(
                """
                Could not find local path for file \(self.metadata.fileName, privacy: .public)
                """
            )
            return (nil, nil, NSFileProviderError(.cannotSynchronize))
        }

        let dbManager = FilesDatabaseManager.shared
        let updatedMetadata = await withCheckedContinuation { continuation in
            dbManager.setStatusForItemMetadata(metadata, status: .downloading) { updatedMeta in
                continuation.resume(returning: updatedMeta)
            }
        }

        guard let updatedMetadata = updatedMetadata else {
            Self.logger.error(
                """
                Could not acquire updated metadata of item \(ocId, privacy: .public),
                unable to update item status to downloading
                """
            )
            return (nil, nil, NSFileProviderError(.noSuchItem))
        }

        let (etag, date, error) = await withCheckedContinuation { continuation in
            self.ncKit.download(
                serverUrlFileName: serverUrlFileName,
                fileNameLocalPath: fileNameLocalPath.path,
                requestHandler: {  progress.setHandlersFromAfRequest($0) },
                taskHandler: { task in
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                },
                progressHandler: { downloadProgress in
                    downloadProgress.copyCurrentStateToProgress(progress)
                }
            ) { _, etag, date, _, _, _, error in
                continuation.resume(returning: (etag, date, error))
            }
        }

        if let fpError = error.fileProviderError {
            Self.logger.error(
                """
                Could not acquire contents of item with identifier: \(ocId, privacy: .public)
                and fileName: \(updatedMetadata.fileName, privacy: .public)
                at \(serverUrlFileName, privacy: .public)
                error: \(fpError.localizedDescription, privacy: .public)
                """
            )

            updatedMetadata.status = ItemMetadata.Status.downloadError.rawValue
            updatedMetadata.sessionError = error.errorDescription
            dbManager.addItemMetadata(updatedMetadata)
            return (nil, nil, fpError)
        }

        Self.logger.debug(
            """
            Acquired contents of item with identifier: \(ocId, privacy: .public)
            and filename: \(updatedMetadata.fileName, privacy: .public)
            """
        )

        updatedMetadata.status = ItemMetadata.Status.normal.rawValue
        updatedMetadata.sessionError = ""
        updatedMetadata.date = (date ?? NSDate()) as Date
        updatedMetadata.etag = etag ?? ""

        dbManager.addLocalFileMetadataFromItemMetadata(updatedMetadata)
        dbManager.addItemMetadata(updatedMetadata)

        guard let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(
            updatedMetadata
        ) else { return (nil, nil, NSFileProviderError(.noSuchItem)) }

        let fpItem = Item(
            metadata: updatedMetadata,
            parentItemIdentifier: parentItemIdentifier,
            ncKit: self.ncKit
        )

        return (fileNameLocalPath, fpItem, nil)
    }

    func move(
        newFileName: String,
        newRemotePath: String,
        newParentItemIdentifier: NSFileProviderItemIdentifier,
        newParentItemRemotePath: String
    ) async -> (Item?, Error?) {
        let ocId = itemIdentifier.rawValue
        let isFolder = contentType == .folder || contentType == .directory
        let oldRemotePath = metadata.serverUrl + "/" + metadata.fileName
        let moveError = await withCheckedContinuation { continuation in
            self.ncKit.moveFileOrFolder(
                serverUrlFileNameSource: oldRemotePath,
                serverUrlFileNameDestination: newRemotePath,
                overwrite: false
            ) { _, error in
                continuation.resume(returning: error.fileProviderError)
            }
        }

        guard moveError == nil else {
            Self.logger.error(
                """
                Could not move file or folder: \(oldRemotePath, privacy: .public)
                to \(newRemotePath, privacy: .public),
                received error: \(moveError?.localizedDescription ?? "", privacy: .public)
                """
            )
            return (nil, moveError)
        }

        // Remember that a folder metadata's serverUrl is its direct server URL, while for
        // an item metadata the server URL is the parent folder's URL
        let dbManager = FilesDatabaseManager.shared
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
            ncKit: self.ncKit
        )
        return (modifiedItem, nil)
    }

    private func modifyContents(
        contents newContents: URL?,
        remotePath: String,
        parentItemRemotePath: String,
        domain: NSFileProviderDomain?,
        progress: Progress
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

        let dbManager = FilesDatabaseManager.shared
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

        let (account, etag, date, size, error) = await withCheckedContinuation { continuation in
            self.ncKit.upload(
                serverUrlFileName: remotePath,
                fileNameLocalPath: localPath,
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
                progressHandler: { uploadProgress in
                    uploadProgress.copyCurrentStateToProgress(progress)
                }
            ) { account, _, etag, date, size, _, _, error in
                continuation.resume(
                    returning: (account, etag, date, size, error.fileProviderError)
                )
            }
        }

        guard error == nil else {
            Self.logger.error(
                """
                Could not upload item \(ocId, privacy: .public)
                with filename: \(self.filename, privacy: .public),
                received error: \(error?.localizedDescription ?? "", privacy: .public)
                """
            )

            metadata.status = ItemMetadata.Status.uploadError.rawValue
            metadata.sessionError = error?.localizedDescription ?? ""
            dbManager.addItemMetadata(metadata)
            return (nil, error)
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
                Created item upload reported as successful,
                but there are differences between the received file size (\(size, privacy: .public))
                and the original file size (\(self.documentSize?.int64Value ?? 0))
                """
            )
        }

        let newMetadata = ItemMetadata()
        newMetadata.date = (date ?? NSDate()) as Date
        newMetadata.etag = etag ?? ""
        newMetadata.account = account
        newMetadata.fileName = self.filename
        newMetadata.fileNameView = self.filename
        newMetadata.ocId = ocId
        newMetadata.size = size
        newMetadata.contentType = self.contentType.preferredMIMEType ?? ""
        newMetadata.directory = self.metadata.directory
        newMetadata.serverUrl = parentItemRemotePath
        newMetadata.session = ""
        newMetadata.sessionError = ""
        newMetadata.sessionTaskIdentifier = 0
        newMetadata.status = ItemMetadata.Status.normal.rawValue

        dbManager.addLocalFileMetadataFromItemMetadata(newMetadata)
        dbManager.addItemMetadata(newMetadata)

        let modifiedItem = Item(
            metadata: newMetadata,
            parentItemIdentifier: parentItemIdentifier,
            ncKit: self.ncKit
        )
        return (modifiedItem, nil)
    }

    public func modify(
        itemTarget: NSFileProviderItem,
        baseVersion: NSFileProviderItemVersion = NSFileProviderItemVersion(),
        changedFields: NSFileProviderItemFields,
        contents newContents: URL?,
        options: NSFileProviderModifyItemOptions = [],
        request: NSFileProviderRequest = NSFileProviderRequest(),
        ncAccount: Account,
        domain: NSFileProviderDomain? = nil,
        progress: Progress
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
        let dbManager = FilesDatabaseManager.shared
        let isFolder = contentType == .folder || contentType == .directory

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
                newParentItemRemotePath: parentItemServerUrl
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

            let (contentModifiedItem, contentError) = await modifiedItem.modifyContents(
                contents: newContents,
                remotePath: newServerUrlFileName,
                parentItemRemotePath: parentItemServerUrl,
                domain: domain,
                progress: progress
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

    public func delete() async -> Error? {
        let serverFileNameUrl = metadata.serverUrl + "/" + metadata.fileName
        guard serverFileNameUrl != "" else {
            return NSFileProviderError(.noSuchItem)
        }
        let ocId = itemIdentifier.rawValue

        let error = await withCheckedContinuation { 
            (continuation: CheckedContinuation<Error?, Never>) -> Void in
            ncKit.deleteFileOrFolder(serverUrlFileName: serverFileNameUrl) { _, error in
                continuation.resume(returning: error.fileProviderError)
            }
        }

        guard error == nil else {
            Self.logger.error(
                """
                Could not delete item with ocId \(ocId, privacy: .public)...
                at \(serverFileNameUrl, privacy: .public)...
                received error: \(error?.localizedDescription ?? "", privacy: .public)
                """
            )
            return error
        }

        Self.logger.info(
            """
            Successfully deleted item with identifier: \(ocId, privacy: .public)...
            at: \(serverFileNameUrl, privacy: .public)
            """
        )

        let dbManager = FilesDatabaseManager.shared

        if self.metadata.directory {
            _ = dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: ocId)
        } else {
            dbManager.deleteItemMetadata(ocId: ocId)
            if dbManager.localFileMetadataFromOcId(ocId) != nil {
                dbManager.deleteLocalFileMetadata(ocId: ocId)
            }
        }
        return nil
    }
}

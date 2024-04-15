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
                received error: \(createError, privacy: .public)
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
                received error: \(readError, privacy: .public)
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
        let (account, ocId, etag, date, size, error) = await withCheckedContinuation { continuation in
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
                received error: \(error, privacy: .public)
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
        fields: NSFileProviderItemFields,
        contents url: URL?,
        options: NSFileProviderCreateItemOptions = [],
        request: NSFileProviderRequest,
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
                    could not find metadata for parentItemIdentifier \(parentItemIdentifier.rawValue, privacy: .public)
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
                received error: \(error, privacy: .public)
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
                received error: \(error, privacy: .public)
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

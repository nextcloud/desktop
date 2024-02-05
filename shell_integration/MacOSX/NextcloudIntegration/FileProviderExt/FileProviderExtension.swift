/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
import NCDesktopClientSocketKit
import NextcloudKit
import OSLog

@objc class FileProviderExtension: NSObject, NSFileProviderReplicatedExtension, NKCommonDelegate {
    let domain: NSFileProviderDomain
    let ncKit = NextcloudKit()
    let appGroupIdentifier = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix") as? String
    var ncAccount: NextcloudAccount?
    lazy var ncKitBackground = NKBackground(nkCommonInstance: ncKit.nkCommonInstance)
    lazy var socketClient: LocalSocketClient? = {
        guard let containerUrl = pathForAppGroupContainer() else {
            Logger.fileProviderExtension.critical("Won't start socket client, no container url")
            return nil;
        }

        let socketPath = containerUrl.appendingPathComponent(
            ".fileprovidersocket", conformingTo: .archive)
        let lineProcessor = FileProviderSocketLineProcessor(delegate: self)
        return LocalSocketClient(socketPath: socketPath.path, lineProcessor: lineProcessor)
    }()

    let urlSessionIdentifier = "com.nextcloud.session.upload.fileproviderext"
    let urlSessionMaximumConnectionsPerHost = 5
    lazy var urlSession: URLSession = {
        let configuration = URLSessionConfiguration.background(withIdentifier: urlSessionIdentifier)
        configuration.allowsCellularAccess = true
        configuration.sessionSendsLaunchEvents = true
        configuration.isDiscretionary = false
        configuration.httpMaximumConnectionsPerHost = urlSessionMaximumConnectionsPerHost
        configuration.requestCachePolicy = NSURLRequest.CachePolicy.reloadIgnoringLocalCacheData
        configuration.sharedContainerIdentifier = appGroupIdentifier

        let session = URLSession(
            configuration: configuration,
            delegate: ncKitBackground,
            delegateQueue: OperationQueue.main
        )
        return session
    }()

    // Whether or not we are going to recursively scan new folders when they are discovered.
    // Apple's recommendation is that we should always scan the file hierarchy fully.
    // This does lead to long load times when a file provider domain is initially configured.
    // We can instead do a fast enumeration where we only scan folders as the user navigates through
    // them, thereby avoiding this issue; the trade-off is that we will be unable to detect
    // materialised file moves to unexplored folders, therefore deleting the item when we could have
    // just moved it instead.
    //
    // Since it's not desirable to cancel a long recursive enumeration half-way through, we do the
    // fast enumeration by default. We prompt the user on the client side to run a proper, full
    // enumeration if they want for safety.
    lazy var config = FileProviderConfig(domainIdentifier: domain.identifier)

    required init(domain: NSFileProviderDomain) {
        // The containing application must create a domain using 
        // `NSFileProviderManager.add(_:, completionHandler:)`. The system will then launch the
        // application extension process, call `FileProviderExtension.init(domain:)` to instantiate
        // the extension for that domain, and call methods on the instance.
        self.domain = domain
        super.init()
        socketClient?.start()
    }

    func invalidate() {
        // TODO: cleanup any resources
        Logger.fileProviderExtension.debug(
            "Extension for domain \(self.domain.displayName, privacy: .public) is being torn down"
        )
    }

    // MARK: NSFileProviderReplicatedExtension protocol methods

    func item(
        for identifier: NSFileProviderItemIdentifier, request _: NSFileProviderRequest,
        completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void
    ) -> Progress {
        // resolve the given identifier to a record in the model

        Logger.fileProviderExtension.debug(
            "Received item request for item with identifier: \(identifier.rawValue, privacy: .public)"
        )
        if identifier == .rootContainer {
            guard let ncAccount else {
                Logger.fileProviderExtension.error(
                    "Not providing item: \(identifier.rawValue, privacy: .public) as account not set up yet"
                )
                completionHandler(nil, NSFileProviderError(.notAuthenticated))
                return Progress()
            }

            let metadata = NextcloudItemMetadataTable()

            metadata.account = ncAccount.ncKitAccount
            metadata.directory = true
            metadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue
            metadata.fileName = "root"
            metadata.fileNameView = "root"
            metadata.serverUrl = ncAccount.serverUrl
            metadata.classFile = NKCommon.TypeClassFile.directory.rawValue

            completionHandler(
                FileProviderItem(
                    metadata: metadata,
                    parentItemIdentifier: NSFileProviderItemIdentifier.rootContainer,
                    ncKit: ncKit), nil)
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared

        guard let metadata = dbManager.itemMetadataFromFileProviderItemIdentifier(identifier),
            let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(metadata)
        else {
            completionHandler(nil, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        completionHandler(
            FileProviderItem(
                metadata: metadata, parentItemIdentifier: parentItemIdentifier, ncKit: ncKit), nil)
        return Progress()
    }

    func fetchContents(
        for itemIdentifier: NSFileProviderItemIdentifier,
        version requestedVersion: NSFileProviderItemVersion?, request: NSFileProviderRequest,
        completionHandler: @escaping (URL?, NSFileProviderItem?, Error?) -> Void
    ) -> Progress {
        Logger.fileProviderExtension.debug(
            "Received request to fetch contents of item with identifier: \(itemIdentifier.rawValue, privacy: .public)"
        )

        guard requestedVersion == nil else {
            // TODO: Add proper support for file versioning
            Logger.fileProviderExtension.error(
                "Can't return contents for specific version as this is not supported.")
            completionHandler(
                nil, nil,
                NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo: [:]))
            return Progress()
        }

        guard ncAccount != nil else {
            Logger.fileProviderExtension.error(
                "Not fetching contents item: \(itemIdentifier.rawValue, privacy: .public) as account not set up yet"
            )
            completionHandler(nil, nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let ocId = itemIdentifier.rawValue
        guard let metadata = dbManager.itemMetadataFromOcId(ocId) else {
            Logger.fileProviderExtension.error(
                "Could not acquire metadata of item with identifier: \(itemIdentifier.rawValue, privacy: .public)"
            )
            completionHandler(nil, nil, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        guard !metadata.isDocumentViewableOnly else {
            Logger.fileProviderExtension.error(
                "Could not get contents of item as is readonly: \(itemIdentifier.rawValue, privacy: .public) \(metadata.fileName, privacy: .public)"
            )
            completionHandler(nil, nil, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        let serverUrlFileName = metadata.serverUrl + "/" + metadata.fileName

        Logger.fileProviderExtension.debug(
            "Fetching file with name \(metadata.fileName, privacy: .public) at URL: \(serverUrlFileName, privacy: .public)"
        )

        let progress = Progress()

        // TODO: Handle folders nicely
        do {
            let fileNameLocalPath = try localPathForNCFile(
                ocId: metadata.ocId, fileNameView: metadata.fileNameView, domain: domain)

            dbManager.setStatusForItemMetadata(
                metadata, status: NextcloudItemMetadataTable.Status.downloading
            ) { updatedMetadata in

                guard let updatedMetadata else {
                    Logger.fileProviderExtension.error(
                        "Could not acquire updated metadata of item with identifier: \(itemIdentifier.rawValue, privacy: .public), unable to update item status to downloading"
                    )
                    completionHandler(nil, nil, NSFileProviderError(.noSuchItem))
                    return
                }

                self.ncKit.download(
                    serverUrlFileName: serverUrlFileName,
                    fileNameLocalPath: fileNameLocalPath.path,
                    requestHandler: { request in
                        progress.setHandlersFromAfRequest(request)
                    },
                    taskHandler: { task in
                        NSFileProviderManager(for: self.domain)?.register(
                            task, forItemWithIdentifier: itemIdentifier, completionHandler: { _ in }
                        )
                    },
                    progressHandler: { downloadProgress in
                        downloadProgress.copyCurrentStateToProgress(progress)
                    }
                ) { _, etag, date, _, _, _, error in
                    if error == .success {
                        Logger.fileTransfer.debug(
                            "Acquired contents of item with identifier: \(itemIdentifier.rawValue, privacy: .public) and filename: \(updatedMetadata.fileName, privacy: .public)"
                        )

                        updatedMetadata.status = NextcloudItemMetadataTable.Status.normal.rawValue
                        updatedMetadata.sessionError = ""
                        updatedMetadata.date = (date ?? NSDate()) as Date
                        updatedMetadata.etag = etag ?? ""

                        dbManager.addLocalFileMetadataFromItemMetadata(updatedMetadata)
                        dbManager.addItemMetadata(updatedMetadata)

                        guard
                            let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(
                                updatedMetadata)
                        else {
                            completionHandler(nil, nil, NSFileProviderError(.noSuchItem))
                            return
                        }

                        let fpItem = FileProviderItem(
                            metadata: updatedMetadata, parentItemIdentifier: parentItemIdentifier,
                            ncKit: self.ncKit)

                        completionHandler(fileNameLocalPath, fpItem, nil)
                    } else {
                        Logger.fileTransfer.error(
                            "Could not acquire contents of item with identifier: \(itemIdentifier.rawValue, privacy: .public) and fileName: \(updatedMetadata.fileName, privacy: .public)"
                        )

                        updatedMetadata.status =
                            NextcloudItemMetadataTable.Status.downloadError.rawValue
                        updatedMetadata.sessionError = error.errorDescription

                        dbManager.addItemMetadata(updatedMetadata)

                        completionHandler(nil, nil, error.fileProviderError)
                    }
                }
            }
        } catch {
            Logger.fileProviderExtension.error(
                "Could not find local path for file \(metadata.fileName, privacy: .public), received error: \(error.localizedDescription, privacy: .public)"
            )
            completionHandler(nil, nil, NSFileProviderError(.cannotSynchronize))
        }

        return progress
    }

    func createItem(
        basedOn itemTemplate: NSFileProviderItem, fields _: NSFileProviderItemFields,
        contents url: URL?, options: NSFileProviderCreateItemOptions = [],
        request: NSFileProviderRequest,
        completionHandler: @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?)
            ->
            Void
    ) -> Progress {
        // TODO: a new item was created on disk, process the item's creation

        Logger.fileProviderExtension.debug(
            "Received create item request for item with identifier: \(itemTemplate.itemIdentifier.rawValue, privacy: .public) and filename: \(itemTemplate.filename, privacy: .public)"
        )

        guard itemTemplate.contentType != .symbolicLink else {
            Logger.fileProviderExtension.error("Cannot create item, symbolic links not supported.")
            completionHandler(
                itemTemplate, NSFileProviderItemFields(), false,
                NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo: [:]))
            return Progress()
        }

        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not creating item: \(itemTemplate.itemIdentifier.rawValue, privacy: .public) as account not set up yet"
            )
            completionHandler(
                itemTemplate, NSFileProviderItemFields(), false,
                NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let parentItemIdentifier = itemTemplate.parentItemIdentifier
        let itemTemplateIsFolder =
            itemTemplate.contentType == .folder || itemTemplate.contentType == .directory

        if options.contains(.mayAlreadyExist) {
            // TODO: This needs to be properly handled with a check in the db
            Logger.fileProviderExtension.info(
                "Not creating item: \(itemTemplate.itemIdentifier.rawValue, privacy: .public) as it may already exist"
            )
            completionHandler(
                itemTemplate, NSFileProviderItemFields(), false, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        var parentItemServerUrl: String

        if parentItemIdentifier == .rootContainer {
            parentItemServerUrl = ncAccount.davFilesUrl
        } else {
            guard
                let parentItemMetadata = dbManager.directoryMetadata(
                    ocId: parentItemIdentifier.rawValue)
            else {
                Logger.fileProviderExtension.error(
                    "Not creating item: \(itemTemplate.itemIdentifier.rawValue, privacy: .public), could not find metadata for parentItemIdentifier \(parentItemIdentifier.rawValue, privacy: .public)"
                )
                completionHandler(
                    itemTemplate, NSFileProviderItemFields(), false,
                    NSFileProviderError(.noSuchItem))
                return Progress()
            }

            parentItemServerUrl = parentItemMetadata.serverUrl + "/" + parentItemMetadata.fileName
        }

        let fileNameLocalPath = url?.path ?? ""
        let newServerUrlFileName = parentItemServerUrl + "/" + itemTemplate.filename

        Logger.fileProviderExtension.debug(
            "About to upload item with identifier: \(itemTemplate.itemIdentifier.rawValue, privacy: .public) of type: \(itemTemplate.contentType?.identifier ?? "UNKNOWN") (is folder: \(itemTemplateIsFolder ? "yes" : "no") and filename: \(itemTemplate.filename) to server url: \(newServerUrlFileName, privacy: .public) with contents located at: \(fileNameLocalPath, privacy: .public)"
        )

        if itemTemplateIsFolder {
            ncKit.createFolder(serverUrlFileName: newServerUrlFileName) { account, _, _, error in
                guard error == .success else {
                    Logger.fileTransfer.error(
                        "Could not create new folder with name: \(itemTemplate.filename, privacy: .public), received error: \(error.errorDescription, privacy: .public)"
                    )
                    completionHandler(itemTemplate, [], false, error.fileProviderError)
                    return
                }

                // Read contents after creation
                self.ncKit.readFileOrFolder(
                    serverUrlFileName: newServerUrlFileName, depth: "0", showHiddenFiles: true
                ) { account, files, _, error in
                    guard error == .success else {
                        Logger.fileTransfer.error(
                            "Could not read new folder with name: \(itemTemplate.filename, privacy: .public), received error: \(error.errorDescription, privacy: .public)"
                        )
                        return
                    }

                    DispatchQueue.global().async {
                        NextcloudItemMetadataTable.metadatasFromDirectoryReadNKFiles(
                            files, account: account
                        ) {
                            directoryMetadata, _, _ in

                            dbManager.addItemMetadata(directoryMetadata)

                            let fpItem = FileProviderItem(
                                metadata: directoryMetadata,
                                parentItemIdentifier: parentItemIdentifier,
                                ncKit: self.ncKit)

                            completionHandler(fpItem, [], true, nil)
                        }
                    }
                }
            }

            return Progress()
        }

        let progress = Progress()

        ncKit.upload(
            serverUrlFileName: newServerUrlFileName,
            fileNameLocalPath: fileNameLocalPath,
            requestHandler: { request in
                progress.setHandlersFromAfRequest(request)
            },
            taskHandler: { task in
                NSFileProviderManager(for: self.domain)?.register(
                    task, forItemWithIdentifier: itemTemplate.itemIdentifier,
                    completionHandler: { _ in })
            },
            progressHandler: { uploadProgress in
                uploadProgress.copyCurrentStateToProgress(progress)
            }
        ) { account, ocId, etag, date, size, _, _, error in
            guard error == .success, let ocId else {
                Logger.fileTransfer.error(
                    "Could not upload item with filename: \(itemTemplate.filename, privacy: .public), received error: \(error.errorDescription, privacy: .public)"
                )
                completionHandler(itemTemplate, [], false, error.fileProviderError)
                return
            }

            Logger.fileTransfer.info(
                "Successfully uploaded item with identifier: \(ocId, privacy: .public) and filename: \(itemTemplate.filename, privacy: .public)"
            )

            if size != itemTemplate.documentSize as? Int64 {
                Logger.fileTransfer.warning(
                    "Created item upload reported as successful, but there are differences between the received file size (\(size, privacy: .public)) and the original file size (\(itemTemplate.documentSize??.int64Value ?? 0))"
                )
            }

            let newMetadata = NextcloudItemMetadataTable()
            newMetadata.date = (date ?? NSDate()) as Date
            newMetadata.etag = etag ?? ""
            newMetadata.account = account
            newMetadata.fileName = itemTemplate.filename
            newMetadata.fileNameView = itemTemplate.filename
            newMetadata.ocId = ocId
            newMetadata.size = size
            newMetadata.contentType = itemTemplate.contentType?.preferredMIMEType ?? ""
            newMetadata.directory = itemTemplateIsFolder
            newMetadata.serverUrl = parentItemServerUrl
            newMetadata.session = ""
            newMetadata.sessionError = ""
            newMetadata.sessionTaskIdentifier = 0
            newMetadata.status = NextcloudItemMetadataTable.Status.normal.rawValue

            dbManager.addLocalFileMetadataFromItemMetadata(newMetadata)
            dbManager.addItemMetadata(newMetadata)

            let fpItem = FileProviderItem(
                metadata: newMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: self.ncKit
            )

            completionHandler(fpItem, [], false, nil)
        }

        return progress
    }

    func modifyItem(
        _ item: NSFileProviderItem, baseVersion _: NSFileProviderItemVersion,
        changedFields: NSFileProviderItemFields, contents newContents: URL?,
        options: NSFileProviderModifyItemOptions = [], request: NSFileProviderRequest,
        completionHandler: @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?)
            ->
            Void
    ) -> Progress {
        // An item was modified on disk, process the item's modification
        // TODO: Handle finder things like tags, other possible item changed fields

        Logger.fileProviderExtension.debug(
            "Received modify item request for item with identifier: \(item.itemIdentifier.rawValue, privacy: .public) and filename: \(item.filename, privacy: .public)"
        )

        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not modifying item: \(item.itemIdentifier.rawValue, privacy: .public) as account not set up yet"
            )
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let parentItemIdentifier = item.parentItemIdentifier
        let itemTemplateIsFolder = item.contentType == .folder || item.contentType == .directory

        if options.contains(.mayAlreadyExist) {
            // TODO: This needs to be properly handled with a check in the db
            Logger.fileProviderExtension.warning(
                "Modification for item: \(item.itemIdentifier.rawValue, privacy: .public) may already exist"
            )
        }

        var parentItemServerUrl: String

        if parentItemIdentifier == .rootContainer {
            parentItemServerUrl = ncAccount.davFilesUrl
        } else {
            guard
                let parentItemMetadata = dbManager.directoryMetadata(
                    ocId: parentItemIdentifier.rawValue)
            else {
                Logger.fileProviderExtension.error(
                    "Not modifying item: \(item.itemIdentifier.rawValue, privacy: .public), could not find metadata for parentItemIdentifier \(parentItemIdentifier.rawValue, privacy: .public)"
                )
                completionHandler(item, [], false, NSFileProviderError(.noSuchItem))
                return Progress()
            }

            parentItemServerUrl = parentItemMetadata.serverUrl + "/" + parentItemMetadata.fileName
        }

        let fileNameLocalPath = newContents?.path ?? ""
        let newServerUrlFileName = parentItemServerUrl + "/" + item.filename

        Logger.fileProviderExtension.debug(
            "About to upload modified item with identifier: \(item.itemIdentifier.rawValue, privacy: .public) of type: \(item.contentType?.identifier ?? "UNKNOWN") (is folder: \(itemTemplateIsFolder ? "yes" : "no") and filename: \(item.filename, privacy: .public) to server url: \(newServerUrlFileName, privacy: .public) with contents located at: \(fileNameLocalPath, privacy: .public)"
        )

        var modifiedItem = item

        // Create a serial dispatch queue
        // We want to wait for network operations to finish before we fire off subsequent network
        // operations, or we might cause explosions (e.g. trying to modify items that have just been
        // moved elsewhere)
        let dispatchQueue = DispatchQueue(label: "modifyItemQueue", qos: .userInitiated)

        if changedFields.contains(.filename) || changedFields.contains(.parentItemIdentifier) {
            dispatchQueue.async {
                let ocId = item.itemIdentifier.rawValue
                Logger.fileProviderExtension.debug(
                    "Changed fields for item \(ocId, privacy: .public) with filename \(item.filename, privacy: .public) includes filename or parentitemidentifier..."
                )

                guard let metadata = dbManager.itemMetadataFromOcId(ocId) else {
                    Logger.fileProviderExtension.error(
                        "Could not acquire metadata of item with identifier: \(item.itemIdentifier.rawValue, privacy: .public)"
                    )
                    completionHandler(item, [], false, NSFileProviderError(.noSuchItem))
                    return
                }

                var renameError: NSFileProviderError?
                let oldServerUrlFileName = metadata.serverUrl + "/" + metadata.fileName

                let moveFileOrFolderDispatchGroup = DispatchGroup()  // Make this block wait until done
                moveFileOrFolderDispatchGroup.enter()

                self.ncKit.moveFileOrFolder(
                    serverUrlFileNameSource: oldServerUrlFileName,
                    serverUrlFileNameDestination: newServerUrlFileName,
                    overwrite: false
                ) { _, error in
                    guard error == .success else {
                        Logger.fileTransfer.error(
                            "Could not move file or folder: \(oldServerUrlFileName, privacy: .public) to \(newServerUrlFileName, privacy: .public), received error: \(error.errorDescription, privacy: .public)"
                        )
                        renameError = error.fileProviderError
                        moveFileOrFolderDispatchGroup.leave()
                        return
                    }

                    // Remember that a folder metadata's serverUrl is its direct server URL, while for
                    // an item metadata the server URL is the parent folder's URL
                    if itemTemplateIsFolder {
                        _ = dbManager.renameDirectoryAndPropagateToChildren(
                            ocId: ocId, newServerUrl: newServerUrlFileName,
                            newFileName: item.filename)
                        self.signalEnumerator { error in
                            if error != nil {
                                Logger.fileTransfer.error(
                                    "Error notifying change in moved directory: \(error)")
                            }
                        }
                    } else {
                        dbManager.renameItemMetadata(
                            ocId: ocId, newServerUrl: parentItemServerUrl,
                            newFileName: item.filename)
                    }

                    guard let newMetadata = dbManager.itemMetadataFromOcId(ocId) else {
                        Logger.fileTransfer.error(
                            "Could not acquire metadata of item with identifier: \(ocId, privacy: .public), cannot correctly inform of modification"
                        )
                        renameError = NSFileProviderError(.noSuchItem)
                        moveFileOrFolderDispatchGroup.leave()
                        return
                    }

                    modifiedItem = FileProviderItem(
                        metadata: newMetadata, parentItemIdentifier: parentItemIdentifier,
                        ncKit: self.ncKit)
                    moveFileOrFolderDispatchGroup.leave()
                }

                moveFileOrFolderDispatchGroup.wait()

                guard renameError == nil else {
                    Logger.fileTransfer.error(
                        "Stopping rename of item with ocId \(ocId, privacy: .public) due to error: \(renameError!.localizedDescription, privacy: .public)"
                    )
                    completionHandler(modifiedItem, [], false, renameError)
                    return
                }

                guard !itemTemplateIsFolder else {
                    Logger.fileTransfer.debug(
                        "Only handling renaming for folders. ocId: \(ocId, privacy: .public)")
                    completionHandler(modifiedItem, [], false, nil)
                    return
                }
            }

            // Return the progress if item is folder here while the async block runs
            guard !itemTemplateIsFolder else {
                return Progress()
            }
        }

        guard !itemTemplateIsFolder else {
            Logger.fileTransfer.debug(
                "System requested modification for folder with ocID \(item.itemIdentifier.rawValue, privacy: .public) (\(newServerUrlFileName, privacy: .public)) of something other than folder name."
            )
            completionHandler(modifiedItem, [], false, nil)
            return Progress()
        }

        let progress = Progress()

        if changedFields.contains(.contents) {
            dispatchQueue.async {
                Logger.fileProviderExtension.debug(
                    "Item modification for \(item.itemIdentifier.rawValue, privacy: .public) \(item.filename, privacy: .public) includes contents"
                )

                guard newContents != nil else {
                    Logger.fileProviderExtension.warning(
                        "WARNING. Could not upload modified contents as was provided nil contents url. ocId: \(item.itemIdentifier.rawValue, privacy: .public)"
                    )
                    completionHandler(modifiedItem, [], false, NSFileProviderError(.noSuchItem))
                    return
                }

                let ocId = item.itemIdentifier.rawValue
                guard let metadata = dbManager.itemMetadataFromOcId(ocId) else {
                    Logger.fileProviderExtension.error(
                        "Could not acquire metadata of item with identifier: \(ocId, privacy: .public)"
                    )
                    completionHandler(
                        item, NSFileProviderItemFields(), false, NSFileProviderError(.noSuchItem))
                    return
                }

                dbManager.setStatusForItemMetadata(
                    metadata, status: NextcloudItemMetadataTable.Status.uploading
                ) { updatedMetadata in

                    if updatedMetadata == nil {
                        Logger.fileProviderExtension.warning(
                            "Could not acquire updated metadata of item with identifier: \(ocId, privacy: .public), unable to update item status to uploading"
                        )
                    }

                    self.ncKit.upload(
                        serverUrlFileName: newServerUrlFileName,
                        fileNameLocalPath: fileNameLocalPath,
                        requestHandler: { request in
                            progress.setHandlersFromAfRequest(request)
                        },
                        taskHandler: { task in
                            NSFileProviderManager(for: self.domain)?.register(
                                task, forItemWithIdentifier: item.itemIdentifier,
                                completionHandler: { _ in })
                        },
                        progressHandler: { uploadProgress in
                            uploadProgress.copyCurrentStateToProgress(progress)
                        }
                    ) { account, ocId, etag, date, size, _, _, error in
                        if error == .success, let ocId {
                            Logger.fileProviderExtension.info(
                                "Successfully uploaded item with identifier: \(ocId, privacy: .public) and filename: \(item.filename, privacy: .public)"
                            )

                            if size != item.documentSize as? Int64 {
                                Logger.fileTransfer.warning(
                                    "Created item upload reported as successful, but there are differences between the received file size (\(size, privacy: .public)) and the original file size (\(item.documentSize??.int64Value ?? 0))"
                                )
                            }

                            let newMetadata = NextcloudItemMetadataTable()
                            newMetadata.date = (date ?? NSDate()) as Date
                            newMetadata.etag = etag ?? ""
                            newMetadata.account = account
                            newMetadata.fileName = item.filename
                            newMetadata.fileNameView = item.filename
                            newMetadata.ocId = ocId
                            newMetadata.size = size
                            newMetadata.contentType = item.contentType?.preferredMIMEType ?? ""
                            newMetadata.directory = itemTemplateIsFolder
                            newMetadata.serverUrl = parentItemServerUrl
                            newMetadata.session = ""
                            newMetadata.sessionError = ""
                            newMetadata.sessionTaskIdentifier = 0
                            newMetadata.status = NextcloudItemMetadataTable.Status.normal.rawValue

                            dbManager.addLocalFileMetadataFromItemMetadata(newMetadata)
                            dbManager.addItemMetadata(newMetadata)

                            modifiedItem = FileProviderItem(
                                metadata: newMetadata, parentItemIdentifier: parentItemIdentifier,
                                ncKit: self.ncKit
                            )
                            completionHandler(modifiedItem, [], false, nil)
                        } else {
                            Logger.fileTransfer.error(
                                "Could not upload item \(item.itemIdentifier.rawValue, privacy: .public) with filename: \(item.filename, privacy: .public), received error: \(error.errorDescription, privacy: .public)"
                            )

                            metadata.status = NextcloudItemMetadataTable.Status.uploadError.rawValue
                            metadata.sessionError = error.errorDescription

                            dbManager.addItemMetadata(metadata)

                            completionHandler(modifiedItem, [], false, error.fileProviderError)
                            return
                        }
                    }
                }
            }
        } else {
            Logger.fileProviderExtension.debug(
                "Nothing more to do with \(item.itemIdentifier.rawValue, privacy: .public) \(item.filename, privacy: .public), modifications complete"
            )
            completionHandler(modifiedItem, [], false, nil)
        }

        return progress
    }

    func deleteItem(
        identifier: NSFileProviderItemIdentifier, baseVersion _: NSFileProviderItemVersion,
        options _: NSFileProviderDeleteItemOptions = [], request _: NSFileProviderRequest,
        completionHandler: @escaping (Error?) -> Void
    ) -> Progress {
        Logger.fileProviderExtension.debug(
            "Received delete item request for item with identifier: \(identifier.rawValue, privacy: .public)"
        )

        guard ncAccount != nil else {
            Logger.fileProviderExtension.error(
                "Not deleting item: \(identifier.rawValue, privacy: .public) as account not set up yet"
            )
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let ocId = identifier.rawValue
        guard let itemMetadata = dbManager.itemMetadataFromOcId(ocId) else {
            completionHandler(NSFileProviderError(.noSuchItem))
            return Progress()
        }

        let serverFileNameUrl = itemMetadata.serverUrl + "/" + itemMetadata.fileName
        guard serverFileNameUrl != "" else {
            completionHandler(NSFileProviderError(.noSuchItem))
            return Progress()
        }

        ncKit.deleteFileOrFolder(serverUrlFileName: serverFileNameUrl) { _, error in
            guard error == .success else {
                Logger.fileTransfer.error(
                    "Could not delete item with ocId \(identifier.rawValue, privacy: .public) at \(serverFileNameUrl, privacy: .public), received error: \(error.errorDescription, privacy: .public)"
                )
                completionHandler(error.fileProviderError)
                return
            }

            Logger.fileTransfer.info(
                "Successfully deleted item with identifier: \(identifier.rawValue, privacy: .public) at: \(serverFileNameUrl, privacy: .public)"
            )

            if itemMetadata.directory {
                _ = dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: ocId)
            } else {
                dbManager.deleteItemMetadata(ocId: ocId)
                if dbManager.localFileMetadataFromOcId(ocId) != nil {
                    dbManager.deleteLocalFileMetadata(ocId: ocId)
                }
            }

            completionHandler(nil)
        }

        return Progress()
    }

    func enumerator(
        for containerItemIdentifier: NSFileProviderItemIdentifier, request _: NSFileProviderRequest
    ) throws -> NSFileProviderEnumerator {
        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not providing enumerator for container with identifier \(containerItemIdentifier.rawValue, privacy: .public) yet as account not set up"
            )
            throw NSFileProviderError(.notAuthenticated)
        }

        return FileProviderEnumerator(
            enumeratedItemIdentifier: containerItemIdentifier,
            ncAccount: ncAccount,
            ncKit: ncKit,
            fastEnumeration: config.fastEnumerationEnabled
        )
    }

    func materializedItemsDidChange(completionHandler: @escaping () -> Void) {
        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not purging stale local file metadatas, account not set up")
            completionHandler()
            return
        }

        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error(
                "Could not get file provider manager for domain: \(self.domain.displayName, privacy: .public)"
            )
            completionHandler()
            return
        }

        let materialisedEnumerator = fpManager.enumeratorForMaterializedItems()
        let materialisedObserver = FileProviderMaterialisedEnumerationObserver(
            ncKitAccount: ncAccount.ncKitAccount
        ) { _ in
            completionHandler()
        }
        let startingPage = NSFileProviderPage(NSFileProviderPage.initialPageSortedByName as Data)

        materialisedEnumerator.enumerateItems(for: materialisedObserver, startingAt: startingPage)
    }

    func signalEnumerator(completionHandler: @escaping (_ error: Error?) -> Void) {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error(
                "Could not get file provider manager for domain, could not signal enumerator. This might lead to future conflicts."
            )
            return
        }

        fpManager.signalEnumerator(for: .workingSet, completionHandler: completionHandler)
    }
}

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
import OSLog
import NCDesktopClientSocketKit
import NextcloudKit

class FileProviderExtension: NSObject, NSFileProviderReplicatedExtension, NKCommonDelegate {
    let domain: NSFileProviderDomain
    let ncKit = NextcloudKit()
    lazy var ncKitBackground: NKBackground = {
        let nckb = NKBackground(nkCommonInstance: ncKit.nkCommonInstance)
        return nckb
    }()

    let appGroupIdentifier: String? = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix") as? String
    var ncAccount: NextcloudAccount?
    lazy var socketClient: LocalSocketClient? = {
        guard let containerUrl = pathForAppGroupContainer() else {
            Logger.fileProviderExtension.critical("Could not start file provider socket client properly as could not get container url")
            return nil;
        }

        let socketPath = containerUrl.appendingPathComponent(".fileprovidersocket", conformingTo: .archive)
        let lineProcessor = FileProviderSocketLineProcessor(delegate: self)

        return LocalSocketClient(socketPath: socketPath.path, lineProcessor: lineProcessor)
    }()

    let urlSessionIdentifier: String = "com.nextcloud.session.upload.fileproviderext"
    let urlSessionMaximumConnectionsPerHost = 5
    lazy var urlSession: URLSession = {
        let configuration = URLSessionConfiguration.background(withIdentifier: urlSessionIdentifier)
        configuration.allowsCellularAccess = true
        configuration.sessionSendsLaunchEvents = true
        configuration.isDiscretionary = false
        configuration.httpMaximumConnectionsPerHost = urlSessionMaximumConnectionsPerHost
        configuration.requestCachePolicy = NSURLRequest.CachePolicy.reloadIgnoringLocalCacheData
        configuration.sharedContainerIdentifier = appGroupIdentifier

        let session = URLSession(configuration: configuration, delegate: ncKitBackground, delegateQueue: OperationQueue.main)
        return session
    }()
    var outstandingSessionTasks: [String: URLSessionTask] = [:]
    var outstandingOcIdTemp: [String: String] = [:]

    required init(domain: NSFileProviderDomain) {
        self.domain = domain
        // The containing application must create a domain using `NSFileProviderManager.add(_:, completionHandler:)`. The system will then launch the application extension process, call `FileProviderExtension.init(domain:)` to instantiate the extension for that domain, and call methods on the instance.

        super.init()
        self.socketClient?.start()
    }
    
    func invalidate() {
        // TODO: cleanup any resources
        Logger.fileProviderExtension.debug("Extension for domain \(self.domain.displayName) is being torn down")
    }

    // MARK: NSFileProviderReplicatedExtension protocol methods
    
    func item(for identifier: NSFileProviderItemIdentifier, request: NSFileProviderRequest, completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void) -> Progress {
        // resolve the given identifier to a record in the model

        Logger.fileProviderExtension.debug("Received item request for item with identifier: \(identifier.rawValue)")
        if identifier == .rootContainer {
            guard let ncAccount = ncAccount else {
                Logger.fileProviderExtension.error("Not providing item: \(identifier.rawValue) as account not set up yet")
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

            completionHandler(FileProviderItem(metadata: metadata, parentItemIdentifier: NSFileProviderItemIdentifier.rootContainer, ncKit: ncKit), nil)
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        
        guard let metadata = dbManager.itemMetadataFromFileProviderItemIdentifier(identifier),
              let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(metadata) else {
            completionHandler(nil, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        completionHandler(FileProviderItem(metadata: metadata, parentItemIdentifier: parentItemIdentifier, ncKit: ncKit), nil)
        return Progress()
    }
    
    func fetchContents(for itemIdentifier: NSFileProviderItemIdentifier, version requestedVersion: NSFileProviderItemVersion?, request: NSFileProviderRequest, completionHandler: @escaping (URL?, NSFileProviderItem?, Error?) -> Void) -> Progress {

        Logger.fileProviderExtension.debug("Received request to fetch contents of item with identifier: \(itemIdentifier.rawValue)")

        guard requestedVersion == nil else {
            // TODO: Add proper support for file versioning
            Logger.fileProviderExtension.error("Can't return contents for specific version as this is not supported.")
            completionHandler(nil, nil, NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:]))
            return Progress()
        }

        guard ncAccount != nil else {
            Logger.fileProviderExtension.error("Not fetching contents item: \(itemIdentifier.rawValue) as account not set up yet")
            completionHandler(nil, nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let ocId = itemIdentifier.rawValue
        guard let metadata = dbManager.itemMetadataFromOcId(ocId) else {
            Logger.fileProviderExtension.error("Could not acquire metadata of item with identifier: \(itemIdentifier.rawValue)")
            completionHandler(nil, nil, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        guard !metadata.isDocumentViewableOnly else {
            Logger.fileProviderExtension.error("Could not get contents of item as is readonly: \(itemIdentifier.rawValue) \(metadata.fileName)")
            completionHandler(nil, nil, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        let serverUrlFileName = metadata.serverUrl + "/" + metadata.fileName

        Logger.fileProviderExtension.debug("Fetching file with name \(metadata.fileName) at URL: \(serverUrlFileName)")

        let progress = Progress()

        // TODO: Handle folders nicely
        do {
            let fileNameLocalPath = try localPathForNCFile(ocId: metadata.ocId, fileNameView: metadata.fileNameView)

            guard let updatedMetadata = dbManager.setStatusForItemMetadata(metadata, status: NextcloudItemMetadataTable.Status.downloading) else {
                Logger.fileProviderExtension.error("Could not acquire updated metadata of item with identifier: \(itemIdentifier.rawValue)")
                completionHandler(nil, nil, NSFileProviderError(.noSuchItem))
                return Progress()
            }

            self.ncKit.download(serverUrlFileName: serverUrlFileName,
                                fileNameLocalPath: fileNameLocalPath.path,
                                requestHandler: { _ in

            }, taskHandler: { task in
                self.outstandingSessionTasks[serverUrlFileName] = task
                NSFileProviderManager(for: self.domain)?.register(task, forItemWithIdentifier: itemIdentifier, completionHandler: { _ in })
            }, progressHandler: { downloadProgress in
                downloadProgress.copyCurrentStateToProgress(progress)
            }) { _, etag, date, _, _, _, error in
                self.outstandingSessionTasks.removeValue(forKey: serverUrlFileName)

                if error == .success {
                    Logger.fileTransfer.debug("Acquired contents of item with identifier: \(itemIdentifier.rawValue) and filename: \(updatedMetadata.fileName)")
                    updatedMetadata.status = NextcloudItemMetadataTable.Status.normal.rawValue
                    updatedMetadata.date = (date ?? NSDate()) as Date
                    updatedMetadata.etag = etag ?? ""

                    dbManager.addLocalFileMetadataFromItemMetadata(updatedMetadata)
                    dbManager.addItemMetadata(updatedMetadata)

                    guard let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(updatedMetadata) else {
                        completionHandler(nil, nil, NSFileProviderError(.noSuchItem))
                        return
                    }
                    let fpItem = FileProviderItem(metadata: updatedMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: self.ncKit)

                    completionHandler(fileNameLocalPath, fpItem, nil)
                } else {
                    Logger.fileTransfer.error("Could not acquire contents of item with identifier: \(itemIdentifier.rawValue) and fileName: \(updatedMetadata.fileName)")

                    updatedMetadata.status = NextcloudItemMetadataTable.Status.downloadError.rawValue
                    updatedMetadata.sessionError = error.errorDescription

                    dbManager.addItemMetadata(updatedMetadata)

                    completionHandler(nil, nil, error.toFileProviderError())
                }
            }
        } catch let error {
            Logger.fileProviderExtension.error("Could not find local path for file \(metadata.fileName), received error: \(error, privacy: .public)")
            completionHandler(nil, nil, NSFileProviderError(.cannotSynchronize))
        }

        return progress
    }
    
    func createItem(basedOn itemTemplate: NSFileProviderItem, fields: NSFileProviderItemFields, contents url: URL?, options: NSFileProviderCreateItemOptions = [], request: NSFileProviderRequest, completionHandler: @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?) -> Void) -> Progress {
        // TODO: a new item was created on disk, process the item's creation

        Logger.fileProviderExtension.debug("Received create item request for item with identifier: \(itemTemplate.itemIdentifier.rawValue) and filename: \(itemTemplate.filename)")

        guard itemTemplate.contentType != .symbolicLink else {
            Logger.fileProviderExtension.error("Cannot create item, symbolic links not supported.")
            completionHandler(itemTemplate, NSFileProviderItemFields(), false, NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:]))
            return Progress()
        }

        guard let ncAccount = ncAccount else {
            Logger.fileProviderExtension.error("Not creating item: \(itemTemplate.itemIdentifier.rawValue) as account not set up yet")
            completionHandler(itemTemplate, NSFileProviderItemFields(), false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let parentItemIdentifier = itemTemplate.parentItemIdentifier
        let itemTemplateIsFolder = itemTemplate.contentType == .folder ||
                                   itemTemplate.contentType == .directory

        if options.contains(.mayAlreadyExist) {
            // TODO: This needs to be properly handled with a check in the db
            Logger.fileProviderExtension.info("Not creating item: \(itemTemplate.itemIdentifier.rawValue) as it may already exist")
            completionHandler(itemTemplate, NSFileProviderItemFields(), false, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        var parentItemMetadata: NextcloudDirectoryMetadataTable?

        if parentItemIdentifier == .rootContainer {
            let rootMetadata = NextcloudDirectoryMetadataTable()

            rootMetadata.account = ncAccount.ncKitAccount
            rootMetadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue
            rootMetadata.serverUrl = ncAccount.davFilesUrl

            parentItemMetadata = rootMetadata
        } else {
            parentItemMetadata = dbManager.directoryMetadata(ocId: parentItemIdentifier.rawValue)
        }

        guard let parentItemMetadata = parentItemMetadata else {
            Logger.fileProviderExtension.error("Not creating item: \(itemTemplate.itemIdentifier.rawValue), could not find metadata for parentItemIdentifier \(parentItemIdentifier.rawValue)")
            completionHandler(itemTemplate, NSFileProviderItemFields(), false, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        let fileNameLocalPath = url?.path ?? ""
        let newServerUrlFileName = parentItemMetadata.serverUrl + "/" + itemTemplate.filename

        Logger.fileProviderExtension.debug("About to upload item with identifier: \(itemTemplate.itemIdentifier.rawValue) of type: \(itemTemplate.contentType?.identifier ?? "UNKNOWN") (is folder: \(itemTemplateIsFolder ? "yes" : "no") and filename: \(itemTemplate.filename) to server url: \(newServerUrlFileName) with contents located at: \(fileNameLocalPath)")

        if itemTemplateIsFolder {
            self.ncKit.createFolder(serverUrlFileName: newServerUrlFileName) { account, ocId, _, error in
                guard error == .success else {
                    Logger.fileTransfer.error("Could not create new folder with name: \(itemTemplate.filename), received error: \(error, privacy: .public)")
                    completionHandler(itemTemplate, [], false, error.toFileProviderError())
                    return
                }

                // Read contents after creation
                self.ncKit.readFileOrFolder(serverUrlFileName: newServerUrlFileName, depth: "0", showHiddenFiles: true) { account, files, _, error in
                    guard error == .success else {
                        Logger.fileTransfer.error("Could not read new folder with name: \(itemTemplate.filename), received error: \(error, privacy: .public)")
                        return
                    }

                    DispatchQueue.global().async {
                        dbManager.convertNKFilesFromDirectoryReadToItemMetadatas(files, account: account) { directoryMetadata, childDirectoriesMetadata, metadatas in

                            let newDirectoryMetadata = dbManager.directoryMetadataFromItemMetadata(directoryItemMetadata: directoryMetadata)
                            dbManager.addDirectoryMetadata(newDirectoryMetadata)
                            dbManager.addItemMetadata(directoryMetadata)

                            let fpItem = FileProviderItem(metadata: directoryMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: self.ncKit)

                            completionHandler(fpItem, [], true, nil)
                        }
                    }
                }
            }

            return Progress()
        }

        let progress = Progress()

        self.ncKit.upload(serverUrlFileName: newServerUrlFileName,
                          fileNameLocalPath: fileNameLocalPath,
                          requestHandler: { _ in
        }, taskHandler: { task in
            self.outstandingSessionTasks[newServerUrlFileName] = task
            NSFileProviderManager(for: self.domain)?.register(task, forItemWithIdentifier: itemTemplate.itemIdentifier, completionHandler: { _ in })
        }, progressHandler: { uploadProgress in
            uploadProgress.copyCurrentStateToProgress(progress)
        }) { account, ocId, etag, date, size, _, _, error  in
            self.outstandingSessionTasks.removeValue(forKey: newServerUrlFileName)

            guard error == .success, let ocId = ocId/*, size == itemTemplate.documentSize as! Int64*/ else {
                Logger.fileTransfer.error("Could not upload item with filename: \(itemTemplate.filename), received error: \(error, privacy: .public)")
                completionHandler(itemTemplate, [], false, error.toFileProviderError())
                return
            }

            Logger.fileTransfer.info("Successfully uploaded item with identifier: \(ocId) and filename: \(itemTemplate.filename)")

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
            newMetadata.serverUrl = parentItemMetadata.serverUrl
            newMetadata.session = ""
            newMetadata.sessionError = ""
            newMetadata.sessionTaskIdentifier = 0
            newMetadata.status = NextcloudItemMetadataTable.Status.normal.rawValue

            dbManager.addLocalFileMetadataFromItemMetadata(newMetadata)
            dbManager.addItemMetadata(newMetadata)

            let fpItem = FileProviderItem(metadata: newMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: self.ncKit)

            completionHandler(fpItem, [], false, nil)
        }

        return progress
    }
    
    func modifyItem(_ item: NSFileProviderItem, baseVersion version: NSFileProviderItemVersion, changedFields: NSFileProviderItemFields, contents newContents: URL?, options: NSFileProviderModifyItemOptions = [], request: NSFileProviderRequest, completionHandler: @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?) -> Void) -> Progress {
        // An item was modified on disk, process the item's modification
        // TODO: Handle finder things like tags, other possible item changed fields

        Logger.fileProviderExtension.debug("Received modify item request for item with identifier: \(item.itemIdentifier.rawValue) and filename: \(item.filename)")

        guard let ncAccount = ncAccount else {
            Logger.fileProviderExtension.error("Not modifying item: \(item.itemIdentifier.rawValue) as account not set up yet")
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let parentItemIdentifier = item.parentItemIdentifier
        let itemTemplateIsFolder = item.contentType == .folder ||
                                   item.contentType == .directory

        if options.contains(.mayAlreadyExist) {
            // TODO: This needs to be properly handled with a check in the db
            Logger.fileProviderExtension.warning("Modification for item: \(item.itemIdentifier.rawValue) may already exist")
        }

        var parentItemMetadata: NextcloudDirectoryMetadataTable?

        if parentItemIdentifier == .rootContainer {
            let rootMetadata = NextcloudDirectoryMetadataTable()

            rootMetadata.account = ncAccount.ncKitAccount
            rootMetadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue
            rootMetadata.serverUrl = ncAccount.davFilesUrl

            parentItemMetadata = rootMetadata
        } else {
            parentItemMetadata = dbManager.directoryMetadata(ocId: parentItemIdentifier.rawValue)
        }

        guard let parentItemMetadata = parentItemMetadata else {
            Logger.fileProviderExtension.error("Not modifying item: \(item.itemIdentifier.rawValue), could not find metadata for parentItemIdentifier \(parentItemIdentifier.rawValue)")
            completionHandler(item, [], false, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        let fileNameLocalPath = newContents?.path ?? ""
        let newServerUrlFileName = parentItemMetadata.serverUrl + "/" + item.filename

        Logger.fileProviderExtension.debug("About to upload modified item with identifier: \(item.itemIdentifier.rawValue) of type: \(item.contentType?.identifier ?? "UNKNOWN") (is folder: \(itemTemplateIsFolder ? "yes" : "no") and filename: \(item.filename) to server url: \(newServerUrlFileName) with contents located at: \(fileNameLocalPath)")

        var modifiedItem = item

        if changedFields.contains(.filename) || changedFields.contains(.parentItemIdentifier) {
            let ocId = item.itemIdentifier.rawValue
            Logger.fileProviderExtension.debug("Changed fields for item \(ocId) with filename \(item.filename) includes filename or parentitemidentifier...")

            guard let metadata = dbManager.itemMetadataFromOcId(ocId) else {
                Logger.fileProviderExtension.error("Could not acquire metadata of item with identifier: \(item.itemIdentifier.rawValue)")
                completionHandler(item, [], false, NSFileProviderError(.noSuchItem))
                return Progress()
            }

            var renameError: NSFileProviderError?
            let oldServerUrlFileName = metadata.serverUrl + "/" + metadata.fileName

            // We want to wait for network operations to finish before we fire off subsequent network
            // operations, or we might cause explosions (e.g. trying to modify items that have just been
            // moved elsewhere)
            let dispatchGroup = DispatchGroup()
            dispatchGroup.enter()

            self.ncKit.moveFileOrFolder(serverUrlFileNameSource: oldServerUrlFileName,
                                        serverUrlFileNameDestination: newServerUrlFileName,
                                        overwrite: false) { account, error in
                guard error == .success else {
                    Logger.fileTransfer.error("Could not move file or folder: \(oldServerUrlFileName) to \(newServerUrlFileName), received error: \(error, privacy: .public)")
                    renameError = error.toFileProviderError()
                    dispatchGroup.leave()
                    return
                }

                // Remember that a folder metadata's serverUrl is its direct server URL, while for
                // an item metadata the server URL is the parent folder's URL
                if itemTemplateIsFolder {
                    dbManager.renameDirectoryAndPropagateToChildren(ocId: ocId, newServerUrl: newServerUrlFileName, newFileName: item.filename)
                } else {
                    dbManager.renameItemMetadata(ocId: ocId, newServerUrl: parentItemMetadata.serverUrl, newFileName: item.filename)
                }

                guard let newMetadata = dbManager.itemMetadataFromOcId(ocId) else {
                    Logger.fileTransfer.error("Could not acquire metadata of item with identifier: \(ocId), cannot correctly inform of modification")
                    renameError = NSFileProviderError(.noSuchItem)
                    dispatchGroup.leave()
                    return
                }

                modifiedItem = FileProviderItem(metadata: newMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: self.ncKit)
                dispatchGroup.leave()
            }

            dispatchGroup.wait()

            guard renameError == nil else {
                Logger.fileTransfer.error("Stopping rename of item with ocId \(ocId) due to error: \(renameError)")
                completionHandler(modifiedItem, [], false, renameError)
                return Progress()
            }

            guard !itemTemplateIsFolder else {
                Logger.fileTransfer.debug("Only handling renaming for folders. ocId: \(ocId)")
                completionHandler(modifiedItem, [], false, nil)
                return Progress()
            }
        }

        guard !itemTemplateIsFolder else {
            Logger.fileTransfer.debug("System requested modification for folder with ocID \(item.itemIdentifier.rawValue) (\(newServerUrlFileName)) of something other than folder name.")
            completionHandler(modifiedItem, [], false, nil)
            return Progress()
        }

        let progress = Progress()

        if changedFields.contains(.contents) {
            Logger.fileProviderExtension.debug("Item modification for \(item.itemIdentifier.rawValue) \(item.filename) includes contents")

            guard newContents != nil else {
                Logger.fileProviderExtension.warning("WARNING. Could not upload modified contents as was provided nil contents url. ocId: \(item.itemIdentifier.rawValue)")
                completionHandler(modifiedItem, [], false, NSFileProviderError(.noSuchItem))
                return Progress()
            }

            self.ncKit.upload(serverUrlFileName: newServerUrlFileName,
                              fileNameLocalPath: fileNameLocalPath,
                              requestHandler: { _ in
            }, taskHandler: { task in
                self.outstandingSessionTasks[newServerUrlFileName] = task
                NSFileProviderManager(for: self.domain)?.register(task, forItemWithIdentifier: item.itemIdentifier, completionHandler: { _ in })
            }, progressHandler: { uploadProgress in
                uploadProgress.copyCurrentStateToProgress(progress)
            }) { account, ocId, etag, date, size, _, _, error  in
                self.outstandingSessionTasks.removeValue(forKey: newServerUrlFileName)

                guard error == .success, let ocId = ocId/*, size == itemTemplate.documentSize as! Int64*/ else {
                    Logger.fileTransfer.error("Could not upload item \(item.itemIdentifier.rawValue) with filename: \(item.filename), received error: \(error, privacy: .public)")
                    completionHandler(modifiedItem, [], false, error.toFileProviderError())
                    return
                }

                Logger.fileProviderExtension.info("Successfully uploaded item with identifier: \(ocId) and filename: \(item.filename)")

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
                newMetadata.serverUrl = parentItemMetadata.serverUrl
                newMetadata.session = ""
                newMetadata.sessionError = ""
                newMetadata.sessionTaskIdentifier = 0
                newMetadata.status = NextcloudItemMetadataTable.Status.normal.rawValue

                dbManager.addLocalFileMetadataFromItemMetadata(newMetadata)
                dbManager.addItemMetadata(newMetadata)

                modifiedItem = FileProviderItem(metadata: newMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: self.ncKit)
                completionHandler(modifiedItem, [], false, nil)
            }
        } else {
            Logger.fileProviderExtension.debug("Nothing more to do with \(item.itemIdentifier.rawValue) \(item.filename), modifications complete")
            completionHandler(modifiedItem, [], false, nil)
        }

        return progress
    }
    
    func deleteItem(identifier: NSFileProviderItemIdentifier, baseVersion version: NSFileProviderItemVersion, options: NSFileProviderDeleteItemOptions = [], request: NSFileProviderRequest, completionHandler: @escaping (Error?) -> Void) -> Progress {

        Logger.fileProviderExtension.debug("Received delete item request for item with identifier: \(identifier.rawValue)")

        guard ncAccount != nil else {
            Logger.fileProviderExtension.error("Not deleting item: \(identifier.rawValue) as account not set up yet")
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

        self.ncKit.deleteFileOrFolder(serverUrlFileName: serverFileNameUrl) { account, error in
            guard error == .success else {
                Logger.fileTransfer.error("Could not delete item with ocId \(identifier.rawValue) at \(serverFileNameUrl), received error: \(error, privacy: .public)")
                completionHandler(error.toFileProviderError())
                return
            }

            Logger.fileTransfer.info("Successfully deleted item with identifier: \(identifier.rawValue) at: \(serverFileNameUrl)")

            if itemMetadata.directory {
                dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: ocId)
            }

            if dbManager.localFileMetadataFromOcId(ocId) != nil {
                dbManager.deleteLocalFileMetadata(ocId: ocId)
            }

            completionHandler(nil)
        }

        return Progress()
    }
    
    func enumerator(for containerItemIdentifier: NSFileProviderItemIdentifier, request: NSFileProviderRequest) throws -> NSFileProviderEnumerator {

        guard let ncAccount = ncAccount else {
            Logger.fileProviderExtension.error("Not providing enumerator for container with identifier \(containerItemIdentifier.rawValue) yet as account not set up")
            throw NSFileProviderError(.notAuthenticated)
        }

        return FileProviderEnumerator(enumeratedItemIdentifier: containerItemIdentifier, ncAccount: ncAccount, ncKit: ncKit)
    }

    func materializedItemsDidChange(completionHandler: @escaping () -> Void) {
        guard let ncAccount = self.ncAccount else {
            Logger.fileProviderExtension.error("Not purging stale local file metadatas, account not set up")
            completionHandler()
            return
        }

        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error("Could not get file provider manager for domain: \(self.domain.displayName)")
            completionHandler()
            return
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let materialisedEnumerator = fpManager.enumeratorForMaterializedItems()
        let materialisedObserver = FileProviderMaterialisedEnumerationObserver(ncKitAccount: ncAccount.ncKitAccount) { _ in
            completionHandler()
        }
        let startingPage = NSFileProviderPage(NSFileProviderPage.initialPageSortedByName as Data)

        materialisedEnumerator.enumerateItems(for: materialisedObserver, startingAt: startingPage)
    }

    // MARK: Nextcloud desktop client communication
    func sendFileProviderDomainIdentifier() {
        let command = "FILE_PROVIDER_DOMAIN_IDENTIFIER_REQUEST_REPLY"
        let argument = domain.identifier.rawValue
        let message = command + ":" + argument + "\n"
        socketClient?.sendMessage(message)
    }

    private func signalEnumeratorAfterAccountSetup() {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error("Could not get file provider manager for domain \(self.domain.displayName), cannot notify after account setup")
            return
        }

        assert(ncAccount != nil)

        fpManager.signalErrorResolved(NSFileProviderError(.notAuthenticated)) { error in
            if error != nil {
                Logger.fileProviderExtension.error("Error resolving not authenticated, received error: \(error!)")
            }
        }

        Logger.fileProviderExtension.debug("Signalling enumerators for user \(self.ncAccount!.username) at server \(self.ncAccount!.serverUrl)")
        fpManager.signalEnumerator(for: .workingSet) { error in
            if error != nil {
                Logger.fileProviderExtension.error("Error signalling enumerator for working set, received error: \(error!.localizedDescription)")
            }
        }
    }

    func setupDomainAccount(user: String, serverUrl: String, password: String) {
        ncAccount = NextcloudAccount(user: user, serverUrl: serverUrl, password: password)
        ncKit.setup(user: ncAccount!.username,
                     userId: ncAccount!.username,
                     password: ncAccount!.password,
                     urlBase: ncAccount!.serverUrl,
                     userAgent: "Nextcloud-macOS/FileProviderExt",
                     nextcloudVersion: 25,
                     delegate: nil) // TODO: add delegate methods for self

        Logger.fileProviderExtension.info("Nextcloud account set up in File Provider extension for user: \(user) at server: \(serverUrl)")

        signalEnumeratorAfterAccountSetup()
    }

    func removeAccountConfig() {
        Logger.fileProviderExtension.info("Received instruction to remove account data for user \(self.ncAccount!.username) at server \(self.ncAccount!.serverUrl)")
        ncAccount = nil
    }
}

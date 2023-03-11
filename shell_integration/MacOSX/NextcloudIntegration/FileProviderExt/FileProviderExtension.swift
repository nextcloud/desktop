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
            NSLog("Could not start file provider socket client properly as could not get container url")
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
        NSLog("Extension for domain %@ is being torn down", domain.displayName)
    }

    // MARK: NSFileProviderReplicatedExtension protocol methods
    
    func item(for identifier: NSFileProviderItemIdentifier, request: NSFileProviderRequest, completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void) -> Progress {
        // resolve the given identifier to a record in the model

        NSLog("Received item request for item with identifier: %@", identifier.rawValue)
        if identifier == .rootContainer {
            guard let ncAccount = ncAccount else {
                NSLog("Not providing item: %@ as account not set up yet", identifier.rawValue)
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
              let parentItemIdentifier = parentItemIdentifierFromMetadata(metadata) else {
            completionHandler(nil, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        completionHandler(FileProviderItem(metadata: metadata, parentItemIdentifier: parentItemIdentifier, ncKit: ncKit), nil)
        return Progress()
    }
    
    func fetchContents(for itemIdentifier: NSFileProviderItemIdentifier, version requestedVersion: NSFileProviderItemVersion?, request: NSFileProviderRequest, completionHandler: @escaping (URL?, NSFileProviderItem?, Error?) -> Void) -> Progress {

        NSLog("Received request to fetch contents of item with identifier: %@", itemIdentifier.rawValue)

        guard requestedVersion == nil else {
            // TODO: Add proper support for file versioning
            NSLog("Can't return contents for specific version as this is not supported.")
            completionHandler(nil, nil, NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:]))
            return Progress()
        }

        guard ncAccount != nil else {
            NSLog("Not fetching contents item: %@ as account not set up yet", itemIdentifier.rawValue)
            completionHandler(nil, nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let ocId = itemIdentifier.rawValue
        guard let metadata = dbManager.itemMetadataFromOcId(ocId) else {
            NSLog("Could not acquire metadata of item with identifier: %@", itemIdentifier.rawValue)
            completionHandler(nil, nil, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        guard !metadata.isDocumentViewableOnly else {
            NSLog("Could not get contents of item as is readonly: %@ %@", itemIdentifier.rawValue, metadata.fileName)
            completionHandler(nil, nil, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        let serverUrlFileName = metadata.serverUrl + "/" + metadata.fileName

        NSLog("Fetching file with name %@ at URL: %@", metadata.fileName, serverUrlFileName)

        let progress = Progress()

        // TODO: Handle folders nicely
        do {
            let fileNameLocalPath = try localPathForNCFile(ocId: metadata.ocId, fileNameView: metadata.fileNameView)

            guard let updatedMetadata = dbManager.setStatusForItemMetadata(metadata, status: NextcloudItemMetadataTable.Status.downloading) else {
                NSLog("Could not acquire updated metadata of item with identifier: %@", itemIdentifier.rawValue)
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
                    NSLog("Acquired contents of item with identifier: %@ and filename: %@", itemIdentifier.rawValue, updatedMetadata.fileName)
                    updatedMetadata.status = NextcloudItemMetadataTable.Status.normal.rawValue
                    updatedMetadata.date = (date ?? NSDate()) as Date
                    updatedMetadata.etag = etag ?? ""

                    dbManager.addLocalFileMetadataFromItemMetadata(updatedMetadata)
                    dbManager.addItemMetadata(updatedMetadata)

                    guard let parentItemIdentifier = parentItemIdentifierFromMetadata(updatedMetadata) else {
                        completionHandler(nil, nil, NSFileProviderError(.noSuchItem))
                        return
                    }
                    let fpItem = FileProviderItem(metadata: updatedMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: self.ncKit)

                    completionHandler(fileNameLocalPath, fpItem, nil)
                } else {
                    NSLog("Could not acquire contents of item with identifier: %@ and fileName: %@", itemIdentifier.rawValue, updatedMetadata.fileName)

                    updatedMetadata.status = NextcloudItemMetadataTable.Status.downloadError.rawValue
                    updatedMetadata.sessionError = error.errorDescription

                    dbManager.addItemMetadata(updatedMetadata)

                    completionHandler(nil, nil, NSFileProviderError(.cannotSynchronize))
                }
            }
        } catch let error {
            NSLog("Could not find local path for file %@, received error: %@", metadata.fileNameView, error.localizedDescription)
        }

        return progress
    }
    
    func createItem(basedOn itemTemplate: NSFileProviderItem, fields: NSFileProviderItemFields, contents url: URL?, options: NSFileProviderCreateItemOptions = [], request: NSFileProviderRequest, completionHandler: @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?) -> Void) -> Progress {
        // TODO: a new item was created on disk, process the item's creation

        NSLog("Received create item request for item with identifier: %@ and filename: %@", itemTemplate.itemIdentifier.rawValue, itemTemplate.filename)

        guard itemTemplate.contentType != .symbolicLink else {
            NSLog("Cannot create item, symbolic links not supported.")
            completionHandler(itemTemplate, NSFileProviderItemFields(), false, NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:]))
            return Progress()
        }

        guard let ncAccount = ncAccount else {
            NSLog("Not creating item: %@ as account not set up yet", itemTemplate.itemIdentifier.rawValue)
            completionHandler(itemTemplate, NSFileProviderItemFields(), false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let parentItemIdentifier = itemTemplate.parentItemIdentifier
        let itemTemplateIsFolder = itemTemplate.contentType == .folder ||
                                   itemTemplate.contentType == .directory

        if options.contains(.mayAlreadyExist) {
            // TODO: This needs to be properly handled with a check in the db
            NSLog("Not creating item: %@ as it may already exist", itemTemplate.itemIdentifier.rawValue)
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
            NSLog("Not creating item: %@, could not find metadata for parentItemIdentifier %@", itemTemplate.itemIdentifier.rawValue, parentItemIdentifier.rawValue)
            completionHandler(itemTemplate, NSFileProviderItemFields(), false, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        let fileNameLocalPath = url?.path ?? ""
        let newServerUrlFileName = parentItemMetadata.serverUrl + "/" + itemTemplate.filename

        NSLog("About to upload item with identifier: %@ of type: %@ (is folder: %@) and filename: %@ to server url: %@ with contents located at: %@", itemTemplate.itemIdentifier.rawValue, itemTemplate.contentType?.identifier ?? "UNKNOWN", itemTemplateIsFolder ? "yes" : "no", itemTemplate.filename, newServerUrlFileName, fileNameLocalPath)

        if itemTemplateIsFolder {
            self.ncKit.createFolder(serverUrlFileName: newServerUrlFileName) { account, ocId, _, error in
                guard error == .success else {
                    NSLog("Could not create new folder with name: %@, received error: %@", itemTemplate.filename, error.errorDescription)
                    completionHandler(itemTemplate, [], false, NSFileProviderError(.serverUnreachable))
                    return
                }

                self.ncKit.readFileOrFolder(serverUrlFileName: newServerUrlFileName, depth: "0", showHiddenFiles: true) { account, files, _, error in
                    guard error == .success else {
                        NSLog("Could not read new folder with name: %@, received error: %@", itemTemplate.filename, error.errorDescription)
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
                NSLog("Could not upload item with filename: %@, received error: %@", itemTemplate.filename, error.errorDescription)
                completionHandler(itemTemplate, [], false, NSFileProviderError(.cannotSynchronize))
                return
            }

            NSLog("Successfully uploaded item with identifier: %@ and filename: %@", ocId, itemTemplate.filename)

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

        NSLog("Received modify item request for item with identifier: %@ and filename: %@", item.itemIdentifier.rawValue, item.filename)

        guard let ncAccount = ncAccount else {
            NSLog("Not modifying item: %@ as account not set up yet", item.itemIdentifier.rawValue)
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        let parentItemIdentifier = item.parentItemIdentifier
        let itemTemplateIsFolder = item.contentType == .folder ||
                                   item.contentType == .directory

        if options.contains(.mayAlreadyExist) {
            // TODO: This needs to be properly handled with a check in the db
            NSLog("Modification for item: %@ may already exist", item.itemIdentifier.rawValue)
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
            NSLog("Not modifying item: %@, could not find metadata for parentItemIdentifier %@", item.itemIdentifier.rawValue, parentItemIdentifier.rawValue)
            completionHandler(item, [], false, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        let fileNameLocalPath = newContents?.path ?? ""
        let newServerUrlFileName = parentItemMetadata.serverUrl + "/" + item.filename

        NSLog("About to upload item with identifier: %@ of type: %@ (is folder: %@) and filename: %@ to server url: %@ with contents located at: %@", item.itemIdentifier.rawValue, item.contentType?.identifier ?? "UNKNOWN", itemTemplateIsFolder ? "yes" : "no", item.filename, newServerUrlFileName, fileNameLocalPath)

        var modifiedItem = item

        if changedFields.contains(.filename) || changedFields.contains(.parentItemIdentifier) {
            NSLog("Changed fields for item with filename %@ includes filename or parentitemidentifier...", item.filename)
            let ocId = item.itemIdentifier.rawValue

            guard let metadata = dbManager.itemMetadataFromOcId(ocId) else {
                NSLog("Could not acquire metadata of item with identifier: %@", ocId)
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
                    NSLog("Could not move file or folder with name: %@, received error: %@", item.filename, error.errorDescription)
                    renameError = NSFileProviderError(.serverUnreachable)
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
                    NSLog("Could not acquire metadata of item with identifier: %@", ocId)
                    renameError = NSFileProviderError(.noSuchItem)
                    dispatchGroup.leave()
                    return
                }

                modifiedItem = FileProviderItem(metadata: newMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: self.ncKit)
                dispatchGroup.leave()
            }

            dispatchGroup.wait()

            guard renameError == nil else {
                NSLog("Stopping rename of item with ocId %@ due to error.", ocId)
                completionHandler(modifiedItem, [], false, renameError)
                return Progress()
            }

            guard !itemTemplateIsFolder else {
                NSLog("Only handling renaming for folders. ocId: %@", modifiedItem.itemIdentifier.rawValue)
                completionHandler(modifiedItem, [], false, nil)
                return Progress()
            }
        }

        guard !itemTemplateIsFolder else {
            NSLog("System requested modification for folder with ocID %@ (%@) of something other than folder name.", item.itemIdentifier.rawValue, newServerUrlFileName)
            completionHandler(modifiedItem, [], false, nil)
            return Progress()
        }

        let progress = Progress()

        if changedFields.contains(.contents) {
            NSLog("Item modification for %@ includes contents", item.filename)

            guard newContents != nil else {
                NSLog("WARNING. Could not upload modified contents as was provided nil contents url. ocId: %@", item.itemIdentifier.rawValue)
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
                    NSLog("Could not upload item with filename: %@, received error: %@", item.filename, error.errorDescription)
                    completionHandler(modifiedItem, [], false, NSFileProviderError(.cannotSynchronize))
                    return
                }

                NSLog("Successfully uploaded item with identifier: %@ and filename: %@", ocId, item.filename)

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
            NSLog("Nothing more to do with %@, modifications complete", item.filename)
            completionHandler(modifiedItem, [], false, nil)
        }

        return progress
    }
    
    func deleteItem(identifier: NSFileProviderItemIdentifier, baseVersion version: NSFileProviderItemVersion, options: NSFileProviderDeleteItemOptions = [], request: NSFileProviderRequest, completionHandler: @escaping (Error?) -> Void) -> Progress {
        // TODO: an item was deleted on disk, process the item's deletion

        NSLog("Received delete item request for item with identifier: %@", identifier.rawValue)

        guard ncAccount != nil else {
            NSLog("Not deleting item: %@ as account not set up yet", identifier.rawValue)
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
                NSLog("Could not delete item with ocId %@ and fileName %@, received error: %@", error.error.localizedDescription)
                completionHandler(NSFileProviderError(.serverUnreachable))
                return
            }

            NSLog("Successfully delete item with identifier: %@ and filename: %@", ocId, serverFileNameUrl)

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
            NSLog("Not providing enumerator for container with identifier %@ yet as account not set up", containerItemIdentifier.rawValue)
            throw NSFileProviderError(.notAuthenticated)
        }

        return FileProviderEnumerator(enumeratedItemIdentifier: containerItemIdentifier, ncAccount: ncAccount, ncKit: ncKit)
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
            NSLog("Could not get file provider manager for domain %@, cannot notify after account setup", domain)
            return
        }

        assert(ncAccount != nil)

        fpManager.signalErrorResolved(NSFileProviderError(.notAuthenticated)) { error in
            if error != nil {
                NSLog("Error resolving not authenticated, received error: %@", error!.localizedDescription)
            }
        }

        guard NextcloudFilesDatabaseManager.shared.anyItemMetadatasForAccount(ncAccount!.ncKitAccount) else {
            // When we have nothing registered, force full refresh.
            // This refreshes the entire structure of the FileProvider and calls
            // enumerateItems rather than enumerateChanges in the enumerator
            NSLog("Signalling manager for user %@ at server %@ to reimport everything", ncAccount!.username, ncAccount!.serverUrl)
            fpManager.reimportItems(below: .rootContainer) { error in
                if error != nil {
                    NSLog("Error reimporting everything, received error: %@", error!.localizedDescription)
                }
            }

            return
        }

        NSLog("Signalling enumerators for user %@ at server %@", ncAccount!.username, ncAccount!.serverUrl)
        fpManager.signalEnumerator(for: .workingSet) { error in
            if error != nil {
                NSLog("Error signalling enumerator for working set, received error: %@", error!.localizedDescription)
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

        NSLog("Nextcloud account set up in File Provider extension for user: %@ at server: %@", user, serverUrl)

        signalEnumeratorAfterAccountSetup()
    }

    func removeAccountConfig() {
        ncAccount = nil
    }
}

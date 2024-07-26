//
//  Item+Create.swift
//
//
//  Created by Claudio Cambra on 16/4/24.
//

import FileProvider
import Foundation
import NextcloudKit
import OSLog

extension Item {
    
    private static func createNewFolder(
        itemTemplate: NSFileProviderItem?,
        remotePath: String,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        domain: NSFileProviderDomain? = nil,
        remoteInterface: RemoteInterface,
        progress: Progress,
        dbManager: FilesDatabaseManager
    ) async -> (Item?, Error?) {

        let (account, _, _, createError) = await remoteInterface.createFolder(
            remotePath: remotePath, options: .init(), taskHandler: { task in
                if let domain, let itemTemplate {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: itemTemplate.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard createError == .success else {
            Self.logger.error(
                """
                Could not create new folder at: \(remotePath, privacy: .public),
                received error: \(createError.errorCode, privacy: .public)
                \(createError.errorDescription, privacy: .public)
                """
            )
            return (
                nil,
                createError.matchesCollisionError ?
                    NSFileProviderError(.filenameCollision) : createError.fileProviderError
            )
        }
        
        // Read contents after creation
        let (_, files, _, readError) = await remoteInterface.enumerate(
            remotePath: remotePath,
            depth: .target,
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            options: .init(),
            taskHandler: { task in
                if let domain, let itemTemplate {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: itemTemplate.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard readError == .success else {
            Self.logger.error(
                """
                Could not read new folder at: \(remotePath, privacy: .public),
                received error: \(readError.errorCode, privacy: .public)
                \(readError.errorDescription, privacy: .public)
                """
            )
            return (nil, readError.fileProviderError)
        }
        
        let directoryMetadata = await withCheckedContinuation { continuation in
            ItemMetadata.metadatasFromDirectoryReadNKFiles(
                files, account: account
            ) { directoryMetadata, _, _ in
                continuation.resume(returning: directoryMetadata)
            }
        }
        
        dbManager.addItemMetadata(directoryMetadata)

        let fpItem = Item(
            metadata: directoryMetadata,
            parentItemIdentifier: parentItemIdentifier,
            remoteInterface: remoteInterface
        )
        
        return (fpItem, nil)
    }
    
    private static func createNewFile(
        remotePath: String,
        localPath: String,
        itemTemplate: NSFileProviderItem,
        parentItemRemotePath: String,
        domain: NSFileProviderDomain? = nil,
        remoteInterface: RemoteInterface,
        progress: Progress,
        dbManager: FilesDatabaseManager
    ) async -> (Item?, Error?) {
        let (account, ocId, etag, date, size, _, _, error) = await remoteInterface.upload(
            remotePath: remotePath,
            localPath: localPath,
            creationDate: itemTemplate.creationDate as? Date,
            modificationDate: itemTemplate.contentModificationDate as? Date,
            options: .init(),
            requestHandler: { progress.setHandlersFromAfRequest($0) },
            taskHandler: { task in
                if let domain = domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: itemTemplate.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            },
            progressHandler: { $0.copyCurrentStateToProgress(progress) }
        )
        
        guard error == .success, let ocId else {
            Self.logger.error(
                """
                Could not upload item with filename: \(itemTemplate.filename, privacy: .public),
                received error: \(error.errorCode, privacy: .public)
                \(error.errorDescription, privacy: .public)
                received ocId: \(ocId ?? "empty", privacy: .public)
                """
            )
            return (
                nil,
                error.matchesCollisionError ?
                    NSFileProviderError(.filenameCollision) : error.fileProviderError
            )
        }
        
        Self.logger.info(
            """
            Successfully uploaded item with identifier: \(ocId, privacy: .public)
            filename: \(itemTemplate.filename, privacy: .public)
            ocId: \(ocId, privacy: .public)
            etag: \(etag ?? "", privacy: .public)
            date: \(date ?? NSDate(), privacy: .public)
            size: \(size, privacy: .public),
            account: \(account, privacy: .public)
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
        
        dbManager.addLocalFileMetadataFromItemMetadata(newMetadata)
        dbManager.addItemMetadata(newMetadata)
        
        let fpItem = Item(
            metadata: newMetadata,
            parentItemIdentifier: itemTemplate.parentItemIdentifier,
            remoteInterface: remoteInterface
        )
        
        return (fpItem, nil)
    }

    @discardableResult private static func createBundleOrPackageInternals(
        rootItem: Item,
        contents: URL,
        remotePath: String,
        domain: NSFileProviderDomain? = nil,
        remoteInterface: RemoteInterface,
        ncAccount: Account,
        progress: Progress,
        dbManager: FilesDatabaseManager
    ) async throws -> Item? {
        Self.logger.debug(
            """
            Handling new bundle/package/internal directory at: \(contents.path, privacy: .public)
            """
        )
        let attributesToFetch: Set<URLResourceKey> = [
            .isDirectoryKey, .fileSizeKey, .creationDateKey, .contentModificationDateKey
        ]
        let fm = FileManager.default
        guard let enumerator = fm.enumerator(
            at: contents, includingPropertiesForKeys: Array(attributesToFetch)
        ) else {
            Self.logger.error(
                """
                Could not create enumerator for contents of bundle or package
                at: \(contents.path, privacy: .public)
                """
            )
            throw NSFileProviderError(.noSuchItem)
        }

        guard let enumeratorArray = enumerator.allObjects as? [URL] else {
            Self.logger.error(
                """
                Could not create enumerator array for contents of bundle or package
                at: \(contents.path, privacy: .public)
                """
            )
            throw NSFileProviderError(.noSuchItem)
        }

        func remoteErrorToThrow(_ error: NKError) -> Error {
            if error.matchesCollisionError {
                return NSFileProviderError(.filenameCollision)
            } else if let error = error.fileProviderError {
                return error
            } else {
                return NSFileProviderError(.cannotSynchronize)
            }
        }

        let contentsPath = contents.path
        let privatePrefix = "/private"
        let privateContentsPath = contentsPath.hasPrefix(privatePrefix)
        var remoteDirectoriesPaths = [remotePath]

        // Add one more total unit count to signify final reconciliation of bundle creation process
        progress.totalUnitCount = Int64(enumeratorArray.count) + 1

        for childUrl in enumeratorArray {
            var childUrlPath = childUrl.path
            if childUrlPath.hasPrefix(privatePrefix), !privateContentsPath {
                childUrlPath.removeFirst(privatePrefix.count)
            }
            let childRelativePath = childUrlPath.replacingOccurrences(of: contents.path, with: "")
            let childRemoteUrl = remotePath + childRelativePath
            let childUrlAttributes = try childUrl.resourceValues(forKeys: attributesToFetch)

            if childUrlAttributes.isDirectory ?? false {
                Self.logger.debug(
                    """
                    Handling child bundle or package directory at: \(childUrlPath, privacy: .public)
                    """
                )
                let (_, _, _, createError) = await remoteInterface.createFolder(
                    remotePath: childRemoteUrl, options: .init(), taskHandler: { task in
                        if let domain {
                            NSFileProviderManager(for: domain)?.register(
                                task,
                                forItemWithIdentifier: rootItem.itemIdentifier,
                                completionHandler: { _ in }
                            )
                        }
                    }
                )

                // As with the creating of the bundle's root folder, we do not want to abort on fail
                // as we might have faced an error creating some other internal content and we want
                // to retry all of its contents
                guard createError == .success || createError.matchesCollisionError else {
                    Self.logger.error(
                        """
                        Could not create new bpi folder at: \(remotePath, privacy: .public),
                        received error: \(createError.errorCode, privacy: .public)
                        \(createError.errorDescription, privacy: .public)
                        """
                    )
                    throw remoteErrorToThrow(createError)
                }
                remoteDirectoriesPaths.append(childRemoteUrl)

            } else {
                Self.logger.debug(
                    """
                    Handling child bundle or package file at: \(childUrlPath, privacy: .public)
                    """
                )
                let (_, _, _, _, _, _, _, error) = await remoteInterface.upload(
                    remotePath: childRemoteUrl,
                    localPath: childUrlPath,
                    creationDate: childUrlAttributes.creationDate,
                    modificationDate: childUrlAttributes.contentModificationDate,
                    options: .init(),
                    requestHandler: { progress.setHandlersFromAfRequest($0) },
                    taskHandler: { task in
                        if let domain {
                            NSFileProviderManager(for: domain)?.register(
                                task,
                                forItemWithIdentifier: rootItem.itemIdentifier,
                                completionHandler: { _ in }
                            )
                        }
                    },
                    progressHandler: { _ in }
                )

                guard error == .success else {
                    Self.logger.error(
                        """
                        Could not upload bpi file at: \(childUrlPath, privacy: .public),
                        received error: \(error.errorCode, privacy: .public)
                        \(error.errorDescription, privacy: .public)
                        """
                    )
                    throw remoteErrorToThrow(error)
                }
            }
            progress.completedUnitCount += 1
        }

        for remoteDirectoryPath in remoteDirectoriesPaths {
            // After everything, check into what the final state is of each folder now
            Self.logger.debug("Reading bpi folder at: \(remoteDirectoryPath, privacy: .public)")
            let (_, _, _, _, readError) = await Enumerator.readServerUrl(
                remoteDirectoryPath,
                ncAccount: ncAccount,
                remoteInterface: remoteInterface,
                dbManager: dbManager
            )

            if let readError, readError != .success {
                Self.logger.error(
                    """
                    Could not read bpi folder at: \(remotePath, privacy: .public),
                    received error: \(readError.errorDescription, privacy: .public)
                    """
                )
                throw remoteErrorToThrow(readError)
            }
        }

        guard let bundleRootMetadata = dbManager.itemMetadata(
            account: ncAccount.ncKitAccount, locatedAtRemoteUrl: remotePath
        ) else {
            Self.logger.error(
                """
                Could not find directory metadata for bundle or package at:
                \(remotePath, privacy: .public)
                of account:
                \(ncAccount.ncKitAccount, privacy: .public)
                with contents located at:
                \(contentsPath, privacy: .public)
                """
            )
            throw NSFileProviderError(.noSuchItem)
        }

        progress.completedUnitCount += 1

        return Item(
            metadata: bundleRootMetadata,
            parentItemIdentifier: rootItem.parentItemIdentifier,
            remoteInterface: remoteInterface
        )
    }

    public static func create(
        basedOn itemTemplate: NSFileProviderItem,
        fields: NSFileProviderItemFields = NSFileProviderItemFields(),
        contents url: URL?,
        options: NSFileProviderCreateItemOptions = [],
        request: NSFileProviderRequest = NSFileProviderRequest(),
        domain: NSFileProviderDomain? = nil,
        remoteInterface: RemoteInterface,
        ncAccount: Account,
        progress: Progress,
        dbManager: FilesDatabaseManager = .shared
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
        
        // TODO: Deduplicate
        if parentItemIdentifier == .rootContainer {
            parentItemRemotePath = ncAccount.davFilesUrl
        } else {
            guard let parentItemMetadata = dbManager.directoryMetadata(
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
        let itemTemplateIsFolder = itemTemplate.contentType?.conforms(to: .directory) ?? false

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

        guard !itemTemplateIsFolder else {
            let isBundleOrPackage =
                itemTemplate.contentType?.conforms(to: .bundle) == true ||
                itemTemplate.contentType?.conforms(to: .package) == true

            var (item, error) = await Self.createNewFolder(
                itemTemplate: itemTemplate,
                remotePath: newServerUrlFileName,
                parentItemIdentifier: parentItemIdentifier,
                domain: domain,
                remoteInterface: remoteInterface,
                progress: isBundleOrPackage ? Progress() : progress,
                dbManager: dbManager
            )

            guard isBundleOrPackage else {
                return (item, error)
            }

            // Ignore collision errors as we might have faced an error creating one of the bundle's
            // internal files or folders and we want to retry all of its contents
            let fpErrorCode = (error as? NSFileProviderError)?.code
            guard error == nil || fpErrorCode == .filenameCollision else {
                Self.logger.error(
                    """
                    Could not create item with identifier: \(tempId, privacy: .public),
                    as it is a bundle or package but could not create the folder.
                    item: \(item, privacy: .public)
                    error: \(error?.localizedDescription ?? "nil", privacy: .public)
                    file provider error code: \(fpErrorCode?.rawValue ?? -1, privacy: .public)
                    """
                )
                return (item, error)
            }

            if item == nil {
                Self.logger.debug(
                    """
                    Item: \(newServerUrlFileName, privacy: .public),
                    is a bundle or package whose root folder already exists, ignoring errors.
                    Fetching remote information and proceeding with creation of internal contents.
                    """
                )
                let (metadatas, _, _, _, readError) = await Enumerator.readServerUrl(
                    newServerUrlFileName,
                    ncAccount: ncAccount,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    domain: domain,
                    depth: .target
                )

                if let readError, readError != .success {
                    Self.logger.error(
                        """
                        Could not read existing bundle or package folder at:
                        \(newServerUrlFileName, privacy: .public),
                        received error: \(readError.errorCode, privacy: .public)
                        \(readError.errorDescription, privacy: .public)
                        """
                    )
                    return (nil, readError.fileProviderError)
                }
                guard let itemMetadata = metadatas?.first else {
                    Self.logger.error(
                        """
                        Could not create item with identifier: \(tempId, privacy: .public),
                        for remotely-existing bundle or package. This should not happen.
                        """
                    )
                    return (nil, NSFileProviderError(.noSuchItem))
                }

                item = Item(
                    metadata: itemMetadata,
                    parentItemIdentifier: parentItemIdentifier,
                    remoteInterface: remoteInterface
                )
            }

            guard let item = item else {
                Self.logger.error(
                    """
                    Could not create item with identifier: \(tempId, privacy: .public),
                    for remotely-existing bundle or package as item is null. This should not happen.
                    """
                )
                return (nil, NSFileProviderError(.noSuchItem))
            }

            guard let url else {
                Self.logger.error(
                    """
                    Could not create item with identifier: \(tempId, privacy: .public),
                    as it is a bundle or package and no contents were provided
                    """
                )
                return (nil, NSFileProviderError(.noSuchItem))
            }

            // Bundles and packages are given to us as if they were files -- i.e. we don't get
            // notified about internal changes. So we need to manually handle their internal
            // contents
            Self.logger.debug(
                """
                Handling bundle or package contents for item: \(tempId, privacy: .public)
                """
            )
            do {
                return (try await Self.createBundleOrPackageInternals(
                    rootItem: item,
                    contents: url,
                    remotePath: newServerUrlFileName,
                    domain: domain,
                    remoteInterface: remoteInterface,
                    ncAccount: ncAccount,
                    progress: progress,
                    dbManager: dbManager
                ), nil)
            } catch {
                return (nil, error)
            }
        }
        
        
        return await Self.createNewFile(
            remotePath: newServerUrlFileName,
            localPath: fileNameLocalPath,
            itemTemplate: itemTemplate,
            parentItemRemotePath: parentItemRemotePath,
            domain: domain,
            remoteInterface: remoteInterface,
            progress: progress,
            dbManager: dbManager
        )
    }
}

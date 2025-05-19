//
//  Item+Create.swift
//
//
//  Created by Claudio Cambra on 16/4/24.
//

import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit
import OSLog

public extension Item {

    private static func createNewFolder(
        itemTemplate: NSFileProviderItem?,
        remotePath: String,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        domain: NSFileProviderDomain? = nil,
        account: Account,
        remoteInterface: RemoteInterface,
        progress: Progress,
        dbManager: FilesDatabaseManager
    ) async -> (Item?, Error?) {

        let (_, _, _, createError) = await remoteInterface.createFolder(
            remotePath: remotePath, account: account, options: .init(), taskHandler: { task in
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
            return (nil, await createError.fileProviderError(
                handlingCollisionAgainstItemInRemotePath: remotePath,
                dbManager: dbManager,
                remoteInterface: remoteInterface
            ))
        }
        
        // Read contents after creation
        let (_, files, _, readError) = await remoteInterface.enumerate(
            remotePath: remotePath,
            depth: .target,
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            account: account,
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
            return (nil, await readError.fileProviderError(
                handlingCollisionAgainstItemInRemotePath: remotePath,
                dbManager: dbManager,
                remoteInterface: remoteInterface
            ))
        }
        
        guard var (directoryMetadata, _, _) = await files.toDirectoryReadMetadatas(account: account)
        else {
            Self.logger.error("Received nil directory read metadatas during conversion")
            return (nil, NSFileProviderError(.cannotSynchronize))
        }
        directoryMetadata.downloaded = true
        dbManager.addItemMetadata(directoryMetadata)

        let fpItem = Item(
            metadata: directoryMetadata,
            parentItemIdentifier: parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: await remoteInterface.supportsTrash(account: account)
        )
        
        return (fpItem, nil)
    }
    
    private static func createNewFile(
        remotePath: String,
        localPath: String,
        itemTemplate: NSFileProviderItem,
        parentItemRemotePath: String,
        domain: NSFileProviderDomain? = nil,
        account: Account,
        remoteInterface: RemoteInterface,
        forcedChunkSize: Int?,
        progress: Progress,
        dbManager: FilesDatabaseManager
    ) async -> (Item?, Error?) {
        let chunkUploadId =
            itemTemplate.itemIdentifier.rawValue.replacingOccurrences(of: "/", with: "")
        let (ocId, _, etag, date, size, _, error) = await upload(
            fileLocatedAt: localPath,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: account,
            inChunksSized: forcedChunkSize,
            usingChunkUploadId: chunkUploadId,
            dbManager: dbManager,
            creationDate: itemTemplate.creationDate as? Date,
            modificationDate: itemTemplate.contentModificationDate as? Date,
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
            return (nil, await error.fileProviderError(
                handlingCollisionAgainstItemInRemotePath: remotePath,
                dbManager: dbManager,
                remoteInterface: remoteInterface
            ))
        }
        
        Self.logger.info(
            """
            Successfully uploaded item with identifier: \(ocId, privacy: .public)
            filename: \(itemTemplate.filename, privacy: .public)
            ocId: \(ocId, privacy: .public)
            etag: \(etag ?? "", privacy: .public)
            date: \(date ?? Date(), privacy: .public)
            size: \(Int(size ?? -1), privacy: .public),
            account: \(account.ncKitAccount, privacy: .public)
            """
        )
        
        if let expectedSize = itemTemplate.documentSize??.int64Value, size != expectedSize {
            Self.logger.warning(
                """
                Created item upload reported as successful, but there are differences between
                the received file size (\(Int(size ?? -1), privacy: .public))
                and the original file size (\(itemTemplate.documentSize??.int64Value ?? 0))
                """
            )
        }
        
        let newMetadata = SendableItemMetadata(
            ocId: ocId,
            account: account.ncKitAccount,
            classFile: "", // Placeholder as not set in original code
            contentType: itemTemplate.contentType?.preferredMIMEType ?? "",
            creationDate: Date(), // Default as not set in original code
            date: date ?? Date(),
            directory: false,
            e2eEncrypted: false, // Default as not set in original code
            etag: etag ?? "",
            fileId: "", // Placeholder as not set in original code
            fileName: itemTemplate.filename,
            fileNameView: itemTemplate.filename,
            hasPreview: false, // Default as not set in original code
            iconName: "", // Placeholder as not set in original code
            mountType: "", // Placeholder as not set in original code
            ownerId: "", // Placeholder as not set in original code
            ownerDisplayName: "", // Placeholder as not set in original code
            path: "", // Placeholder as not set in original code
            serverUrl: parentItemRemotePath,
            size: size ?? 0,
            status: Status.normal.rawValue,
            downloaded: true,
            uploaded: true,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id
        )

        dbManager.addItemMetadata(newMetadata)
        
        let fpItem = Item(
            metadata: newMetadata,
            parentItemIdentifier: itemTemplate.parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: await remoteInterface.supportsTrash(account: account)
        )
        
        return (fpItem, nil)
    }

    @discardableResult private static func createBundleOrPackageInternals(
        rootItem: Item,
        contents: URL,
        remotePath: String,
        domain: NSFileProviderDomain? = nil,
        account: Account,
        remoteInterface: RemoteInterface,
        forcedChunkSize: Int?,
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
            throw NSError(domain: NSURLErrorDomain, code: NSURLErrorResourceUnavailable)
        }

        guard let enumeratorArray = enumerator.allObjects as? [URL] else {
            Self.logger.error(
                """
                Could not create enumerator array for contents of bundle or package
                    at: \(contents.path, privacy: .public)
                """
            )
            throw NSError(domain: NSURLErrorDomain, code: NSURLErrorResourceUnavailable)
        }

        func remoteErrorToThrow(_ error: NKError) -> Error {
            return error.fileProviderError ??  NSFileProviderError(.cannotSynchronize)
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
                    remotePath: childRemoteUrl,
                    account: account,
                    options: .init(), taskHandler: { task in
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
                let (_, _, _, _, _, _, error) = await upload(
                    fileLocatedAt: childUrlPath,
                    toRemotePath: childRemoteUrl,
                    usingRemoteInterface: remoteInterface,
                    withAccount: account,
                    inChunksSized: forcedChunkSize,
                    dbManager: dbManager,
                    creationDate: childUrlAttributes.creationDate,
                    modificationDate: childUrlAttributes.contentModificationDate,
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

                // Do not fail on existing item, just keep going
                guard error == .success || error.matchesCollisionError else {
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
            let (_, _, _, _, _, readError) = await Enumerator.readServerUrl(
                remoteDirectoryPath,
                account: account,
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
            account: account.ncKitAccount, locatedAtRemoteUrl: remotePath
        ) else {
            Self.logger.error(
                """
                Could not find directory metadata for bundle or package at:
                    \(remotePath, privacy: .public)
                    of account:
                    \(account.ncKitAccount, privacy: .public)
                    with contents located at:
                    \(contentsPath, privacy: .public)
                """
            )
            // Yes, it's weird to throw a "non-existent item" error during an item's creation.
            // No, it's not the wrong solution. Thanks to the peculiar way we have to handle bundles
            // things can happen as we are populating the bundle remotely and then checking it.
            throw NSError.fileProviderErrorForNonExistentItem(
                withIdentifier: rootItem.itemIdentifier
            )
        }

        progress.completedUnitCount += 1

        return Item(
            metadata: bundleRootMetadata,
            parentItemIdentifier: rootItem.parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: await remoteInterface.supportsTrash(account: account)
        )
    }

    static func create(
        basedOn itemTemplate: NSFileProviderItem,
        fields: NSFileProviderItemFields = NSFileProviderItemFields(),
        contents url: URL?,
        options: NSFileProviderCreateItemOptions = [],
        request: NSFileProviderRequest = NSFileProviderRequest(),
        domain: NSFileProviderDomain? = nil,
        account: Account,
        remoteInterface: RemoteInterface,
        ignoredFiles: IgnoredFilesMatcher? = nil,
        forcedChunkSize: Int? = nil,
        progress: Progress,
        dbManager: FilesDatabaseManager
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
            return (nil, NSFileProviderError(.cannotSynchronize))
        }

        let parentItemIdentifier = itemTemplate.parentItemIdentifier
        var parentItemRemotePath: String
        var parentItemRelativePath: String

        // TODO: Deduplicate
        if parentItemIdentifier == .rootContainer {
            parentItemRemotePath = account.davFilesUrl
            parentItemRelativePath = "/"
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
                return (nil, NSFileProviderError(.cannotSynchronize))
            }
            parentItemRemotePath = parentItemMetadata.serverUrl + "/" + parentItemMetadata.fileName
            parentItemRelativePath = parentItemRemotePath.replacingOccurrences(
                of: account.davFilesUrl, with: ""
            )
            assert(parentItemRelativePath.starts(with: "/"))
        }

        let itemTemplateIsFolder = itemTemplate.contentType?.conforms(to: .directory) ?? false

        guard !isLockFileName(itemTemplate.filename) || itemTemplateIsFolder else {
            return await Item.createLockFile(
                basedOn: itemTemplate,
                parentItemIdentifier: parentItemIdentifier,
                parentItemRemotePath: parentItemRemotePath,
                progress: progress,
                domain: domain,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager
            )
        }

        let relativePath = parentItemRelativePath + "/" + itemTemplate.filename
        guard ignoredFiles == nil || ignoredFiles?.isExcluded(relativePath) == false else {
            return await Item.createIgnored(
                basedOn: itemTemplate,
                parentItemRemotePath: parentItemRemotePath,
                contents: url,
                account: account,
                remoteInterface: remoteInterface,
                progress: progress,
                dbManager: dbManager
            )
        }

        let fileNameLocalPath = url?.path ?? ""
        let newServerUrlFileName = parentItemRemotePath + "/" + itemTemplate.filename

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
                account: account,
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
                        Fetching remote information, proceeding with creation of internal contents.
                    """
                )
                let (metadatas, _, _, _, _, readError) = await Enumerator.readServerUrl(
                    newServerUrlFileName,
                    account: account,
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
                    return (
                        nil,
                        NSError.fileProviderErrorForNonExistentItem(
                            withIdentifier: itemTemplate.itemIdentifier
                        )
                    )
                }

                item = Item(
                    metadata: itemMetadata,
                    parentItemIdentifier: parentItemIdentifier,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    remoteSupportsTrash: await remoteInterface.supportsTrash(account: account)
                )
            }

            guard let item else {
                Self.logger.error(
                    """
                    Could not create item with identifier: \(tempId, privacy: .public),
                        for remotely-existing bundle or package as item is null.
                        This should not happen!
                    """
                )
                return (nil, NSFileProviderError(.cannotSynchronize))
            }

            guard let url else {
                Self.logger.error(
                    """
                    Could not create item with identifier: \(tempId, privacy: .public),
                        as it is a bundle or package and no contents were provided
                    """
                )
                return (nil, NSError(domain: NSURLErrorDomain, code: NSURLErrorBadURL))
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
                    account: account,
                    remoteInterface: remoteInterface,
                    forcedChunkSize: forcedChunkSize,
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
            account: account,
            remoteInterface: remoteInterface,
            forcedChunkSize: forcedChunkSize,
            progress: progress,
            dbManager: dbManager
        )
    }
}

//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit
import UniformTypeIdentifiers

public extension Item {
    ///
    /// Create a new folder on the server.
    ///
    private static func createNewFolder(
        itemTemplate: NSFileProviderItem?,
        remotePath: String,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        domain: NSFileProviderDomain? = nil,
        account: Account,
        remoteInterface: RemoteInterface,
        progress _: Progress,
        dbManager: FilesDatabaseManager,
        log: any FileProviderLogging
    ) async -> (Item?, Error?) {
        let logger = FileProviderLogger(category: "Item", log: log)

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
            logger.error(
                """
                Could not create new folder at: \(remotePath),
                    received error: \(createError.errorCode)
                    \(createError.errorDescription)
                """
            )
            return await (nil, createError.fileProviderError(
                handlingCollisionAgainstItemInRemotePath: remotePath,
                dbManager: dbManager,
                remoteInterface: remoteInterface,
                log: log
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
            logger.error(
                """
                Could not read new folder at: \(remotePath),
                    received error: \(readError.errorCode)
                    \(readError.errorDescription)
                """
            )
            return await (nil, readError.fileProviderError(
                handlingCollisionAgainstItemInRemotePath: remotePath,
                dbManager: dbManager,
                remoteInterface: remoteInterface,
                log: log
            ))
        }

        guard var (directory, _, _) = await files.toSendableDirectoryMetadata(account: account, directoryToRead: remotePath) else {
            logger.error("Failed to resolve directory metadata on item conversion!")
            return (nil, NSFileProviderError(.cannotSynchronize))
        }

        directory.downloaded = true
        dbManager.addItemMetadata(directory)

        let displayFileActions = await Item.typeHasApplicableContextMenuItems(account: account, remoteInterface: remoteInterface, candidate: directory.contentType)

        let fpItem = await Item(
            metadata: directory,
            parentItemIdentifier: parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: displayFileActions,
            remoteSupportsTrash: remoteInterface.supportsTrash(account: account),
            log: log
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
        dbManager: FilesDatabaseManager,
        log: any FileProviderLogging
    ) async -> (Item?, Error?) {
        let logger = FileProviderLogger(category: "Item", log: log)
        let chunkUploadId =
            itemTemplate.itemIdentifier.rawValue.replacingOccurrences(of: "/", with: "")
        let (ocId, etag, date, size, error) = await upload(
            fileLocatedAt: localPath,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: account,
            inChunksSized: forcedChunkSize,
            usingChunkUploadId: chunkUploadId,
            dbManager: dbManager,
            creationDate: itemTemplate.creationDate as? Date,
            modificationDate: itemTemplate.contentModificationDate as? Date,
            log: log,
            requestHandler: { progress.setHandlersFromAfRequest($0) },
            taskHandler: { task in
                if let domain {
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
            logger.error(
                """
                Could not upload item with filename: \(itemTemplate.filename),
                    received error: \(error.errorCode)
                    \(error.errorDescription)
                    received ocId: \(ocId ?? "empty")
                """
            )
            return await (nil, error.fileProviderError(
                handlingCollisionAgainstItemInRemotePath: remotePath,
                dbManager: dbManager,
                remoteInterface: remoteInterface,
                log: log
            ))
        }

        logger.info(
            """
            Successfully uploaded item with identifier: \(ocId)
            filename: \(itemTemplate.filename)
            ocId: \(ocId)
            etag: \(etag ?? "")
            date: \(date ?? Date())
            size: \(Int(size ?? -1)),
            account: \(account.ncKitAccount)
            """
        )

        if let expectedSize = itemTemplate.documentSize??.int64Value, size != expectedSize {
            logger.info(
                """
                Created item upload reported as successful, but there are differences between
                the received file size (\(Int(size ?? -1)))
                and the original file size (\(itemTemplate.documentSize??.int64Value ?? 0))
                """
            )
        }

        var contentType = ""

        if let preferredMIMEType = itemTemplate.contentType?.preferredMIMEType {
            contentType = preferredMIMEType
        }

        if itemTemplate.contentType == .aliasFile {
            contentType = UTType.aliasFile.identifier
        }

        let newMetadata = SendableItemMetadata(
            ocId: ocId,
            account: account.ncKitAccount,
            classFile: "", // Placeholder as not set in original code
            contentType: contentType,
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

        let displayFileActions = await Item.typeHasApplicableContextMenuItems(account: account, remoteInterface: remoteInterface, candidate: newMetadata.contentType)

        let fpItem = await Item(
            metadata: newMetadata,
            parentItemIdentifier: itemTemplate.parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: displayFileActions,
            remoteSupportsTrash: remoteInterface.supportsTrash(account: account),
            log: log
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
        dbManager: FilesDatabaseManager,
        log: any FileProviderLogging
    ) async throws -> Item? {
        let logger = FileProviderLogger(category: "Item", log: log)

        logger.debug(
            """
            Handling new bundle/package/internal directory at: \(contents.path)
            """
        )
        let attributesToFetch: Set<URLResourceKey> = [
            .isDirectoryKey, .fileSizeKey, .creationDateKey, .contentModificationDateKey
        ]
        let fm = FileManager.default
        guard let enumerator = fm.enumerator(
            at: contents, includingPropertiesForKeys: Array(attributesToFetch)
        ) else {
            logger.error(
                """
                Could not create enumerator for contents of bundle or package
                    at: \(contents.path)
                """
            )
            throw NSError(domain: NSURLErrorDomain, code: NSURLErrorResourceUnavailable)
        }

        guard let enumeratorArray = enumerator.allObjects as? [URL] else {
            logger.error(
                """
                Could not create enumerator array for contents of bundle or package
                    at: \(contents.path)
                """
            )
            throw NSError(domain: NSURLErrorDomain, code: NSURLErrorResourceUnavailable)
        }

        func remoteErrorToThrow(_ error: NKError) -> Error {
            error.fileProviderError ?? NSFileProviderError(.cannotSynchronize)
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
                logger.debug(
                    """
                    Handling child bundle or package directory at: \(childUrlPath)
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
                    logger.error(
                        """
                        Could not create new bpi folder at: \(remotePath),
                        received error: \(createError.errorCode)
                        \(createError.errorDescription)
                        """
                    )
                    throw remoteErrorToThrow(createError)
                }
                remoteDirectoriesPaths.append(childRemoteUrl)

            } else {
                logger.debug(
                    """
                    Handling child bundle or package file at: \(childUrlPath)
                    """
                )
                let (_, _, _, _, error) = await upload(
                    fileLocatedAt: childUrlPath,
                    toRemotePath: childRemoteUrl,
                    usingRemoteInterface: remoteInterface,
                    withAccount: account,
                    inChunksSized: forcedChunkSize,
                    dbManager: dbManager,
                    creationDate: childUrlAttributes.creationDate,
                    modificationDate: childUrlAttributes.contentModificationDate,
                    log: log,
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
                    logger.error(
                        """
                        Could not upload bpi file at: \(childUrlPath),
                        received error: \(error.errorCode)
                        \(error.errorDescription)
                        """
                    )
                    throw remoteErrorToThrow(error)
                }
            }
            progress.completedUnitCount += 1
        }

        for remoteDirectoryPath in remoteDirectoriesPaths {
            // After everything, check into what the final state is of each folder now
            logger.debug("Reading bpi folder at: \(remoteDirectoryPath)")

            let (_, _, _, _, _, readError) = await Enumerator.readServerUrl(
                remoteDirectoryPath,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                log: log
            )

            if let readError, readError != .success {
                logger.error(
                    """
                    Could not read bpi folder at: \(remotePath),
                    received error: \(readError.errorDescription)
                    """
                )
                throw remoteErrorToThrow(readError)
            }
        }

        guard let bundleRootMetadata = dbManager.itemMetadata(
            account: account.ncKitAccount, locatedAtRemoteUrl: remotePath
        ) else {
            logger.error(
                """
                Could not find directory metadata for bundle or package at:
                    \(remotePath)
                    of account:
                    \(account.ncKitAccount)
                    with contents located at:
                    \(contentsPath)
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

        let displayFileActions = await Item.typeHasApplicableContextMenuItems(account: account, remoteInterface: remoteInterface, candidate: bundleRootMetadata.contentType)

        return await Item(
            metadata: bundleRootMetadata,
            parentItemIdentifier: rootItem.parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: displayFileActions,
            remoteSupportsTrash: remoteInterface.supportsTrash(account: account),
            log: log
        )
    }

    static func create(
        basedOn itemTemplate: NSFileProviderItem,
        fields _: NSFileProviderItemFields = NSFileProviderItemFields(),
        contents url: URL?,
        options: NSFileProviderCreateItemOptions = [],
        request _: NSFileProviderRequest = NSFileProviderRequest(),
        domain: NSFileProviderDomain? = nil,
        account: Account,
        remoteInterface: RemoteInterface,
        ignoredFiles: IgnoredFilesMatcher? = nil,
        forcedChunkSize: Int? = nil,
        progress: Progress,
        dbManager: FilesDatabaseManager,
        log: any FileProviderLogging
    ) async -> (Item?, Error?) {
        let logger = FileProviderLogger(category: "Item", log: log)
        let tempId = itemTemplate.itemIdentifier.rawValue

        guard itemTemplate.contentType != .symbolicLink else {
            logger.error(
                "Cannot create item \(tempId), symbolic links not supported."
            )
            return (nil, NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError))
        }

        if options.contains(.mayAlreadyExist) {
            // TODO: This needs to be properly handled with a check in the db
            logger.info(
                """
                Not creating item: \(itemTemplate.itemIdentifier.rawValue)
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
                logger.error(
                    """
                    Not creating item: \(itemTemplate.itemIdentifier.rawValue),
                        could not find metadata for parentItemIdentifier:
                        \(parentItemIdentifier.rawValue)
                    """
                )
                return (nil, NSFileProviderError(.cannotSynchronize))
            }
            parentItemRemotePath = parentItemMetadata.remotePath()
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
                dbManager: dbManager,
                log: log
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
                dbManager: dbManager,
                log: log
            )
        }

        let fileNameLocalPath = url?.path ?? ""
        let newServerUrlFileName = parentItemRemotePath + "/" + itemTemplate.filename

        logger.debug(
            """
            About to upload item with identifier: \(tempId)
            of type: \(itemTemplate.contentType?.identifier ?? "UNKNOWN")
            (is folder: \(itemTemplateIsFolder ? "yes" : "no")
            and filename: \(itemTemplate.filename)
            to server url: \(newServerUrlFileName)
            with contents located at: \(fileNameLocalPath)
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
                dbManager: dbManager,
                log: log
            )

            guard isBundleOrPackage else {
                return (item, error)
            }

            // Ignore collision errors as we might have faced an error creating one of the bundle's
            // internal files or folders and we want to retry all of its contents
            let fpErrorCode = (error as? NSFileProviderError)?.code
            guard error == nil || fpErrorCode == .filenameCollision else {
                logger.error("Could not create item.", [.item: item?.itemIdentifier, .error: error])
                return (item, error)
            }

            if item == nil {
                logger.debug("Item is a bundle or package whose root folder already exists, ignoring errors. Fetching remote information, proceeding with creation of internal contents.")
                let (metadatas, _, _, _, _, readError) = await Enumerator.readServerUrl(
                    newServerUrlFileName,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    domain: domain,
                    depth: .target,
                    log: log
                )

                if let readError, readError != .success {
                    logger.error("Could not read existing bundle or package folder.", [.error: readError, .url: newServerUrlFileName])
                    return (nil, readError.fileProviderError)
                }
                guard let itemMetadata = metadatas?.first else {
                    logger.error("Could not create item for remotely-existing bundle or package. This should not happen.", [.item: tempId])

                    return (
                        nil,
                        NSError.fileProviderErrorForNonExistentItem(
                            withIdentifier: itemTemplate.itemIdentifier
                        )
                    )
                }

                let displayFileActions = await Item.typeHasApplicableContextMenuItems(account: account, remoteInterface: remoteInterface, candidate: itemMetadata.contentType)

                item = await Item(
                    metadata: itemMetadata,
                    parentItemIdentifier: parentItemIdentifier,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    displayFileActions: displayFileActions,
                    remoteSupportsTrash: remoteInterface.supportsTrash(account: account),
                    log: log
                )
            }

            guard let item else {
                logger.error("Could not create item for remotely-existing bundle or package as item is null. This should not happen!", [.item: tempId])
                return (nil, NSFileProviderError(.cannotSynchronize))
            }

            guard let url else {
                logger.error("Could not create item as it is a bundle or package and no contents were provided.", [.item: tempId])
                return (nil, NSError(domain: NSURLErrorDomain, code: NSURLErrorBadURL))
            }

            // Bundles and packages are given to us as if they were files -- i.e. we don't get
            // notified about internal changes. So we need to manually handle their internal
            // contents
            logger.debug("Handling bundle or package contents for item.", [.item: tempId])

            do {
                return try await (Self.createBundleOrPackageInternals(
                    rootItem: item,
                    contents: url,
                    remotePath: newServerUrlFileName,
                    domain: domain,
                    account: account,
                    remoteInterface: remoteInterface,
                    forcedChunkSize: forcedChunkSize,
                    progress: progress,
                    dbManager: dbManager,
                    log: log
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
            dbManager: dbManager,
            log: log
        )
    }
}

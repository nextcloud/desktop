//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudFileProviderXPC
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

        let contentType: String = if itemTemplate.contentType == .aliasFile {
            UTType.aliasFile.identifier
        } else {
            itemTemplate.contentType?.preferredMIMEType ?? ""
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
        appProxy: (any AppProtocol)? = nil,
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

        // Bundles and packages (`.app`, `.key`, `.pages`, `.fcpbundle`, …) are folders that the
        // system presents as atomic files. WebDAV cannot represent the full bundle layout
        // (symlinks, resource forks, permissions); the previous recursive-mirror implementation
        // produced silently incomplete bundles or aborted mid-upload, leaving partial state on
        // the server. Until a proper transport is in place, refuse bundles at the file provider
        // boundary, route them through the existing `createIgnored` path (which returns
        // `NSFileProviderError(.excludedFromSync)`), and tell the user via the activity view.
        // Tracked at https://github.com/nextcloud/desktop/issues/9827.
        let isBundleOrPackage = itemTemplate.contentType?.conforms(to: .bundle) == true || itemTemplate.contentType?.conforms(to: .package) == true

        if isBundleOrPackage {
            logger.info("Refusing to sync bundle or package because this is not supported.", [.name: itemTemplate.filename])

            if let domain {
                BundleExclusionReporter.report(relativePath: parentItemRelativePath + "/" + itemTemplate.filename, fileName: itemTemplate.filename, domainIdentifier: domain.identifier, appProxy: appProxy, log: log)
            }

            return await Item.createIgnored(basedOn: itemTemplate, parentItemRemotePath: parentItemRemotePath, contents: url, account: account, remoteInterface: remoteInterface, progress: progress, dbManager: dbManager, log: log)
        }

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
            // Bundles and packages are filtered out earlier in this function and routed through
            // `Item.createIgnored` (see the bundle-exclusion check above). Anything that still
            // arrives here as a folder is a regular directory.
            return await Self.createNewFolder(
                itemTemplate: itemTemplate,
                remotePath: newServerUrlFileName,
                parentItemIdentifier: parentItemIdentifier,
                domain: domain,
                account: account,
                remoteInterface: remoteInterface,
                progress: progress,
                dbManager: dbManager,
                log: log
            )
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

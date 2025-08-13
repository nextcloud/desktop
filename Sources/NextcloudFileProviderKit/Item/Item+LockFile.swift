//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudCapabilitiesKit

extension Item {
    ///
    /// Create a lock file in the local file provider extension database which is not synchronized to the server, if the server supports file locking.
    ///
    /// - Parameters:
    ///     - basedOn: Passed through as received from the file provider framework.
    ///     - parentItemIdentifier: Passed through as received from the file provider framework.
    ///     - parentItemRemotePath: Passed through as received from the file provider framework.
    ///     - progress: Passed through as received from the file provider framework.
    ///     - domain: File provider domain with which the background network task should be associated with.
    ///     - account: The Nextcloud account to use for interaction with the server.
    ///     - remoteInterface: The server API abstraction to use for calls.
    ///     - dbManager: The database manager to use for managing metadata.
    ///
    /// - Returns: Either the created `item` or an `error` but not both. In either case the other value is `nil`. To be passed to the completion handler provided by the file provider framework.
    ///
    static func createLockFile(
        basedOn itemTemplate: NSFileProviderItem,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        parentItemRemotePath: String,
        progress: Progress,
        domain: NSFileProviderDomain? = nil,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        log: any FileProviderLogging
    ) async -> (Item?, Error?) {
        let logger = FileProviderLogger(category: "Item", log: log)
        progress.totalUnitCount = 1

        // Lock but don't upload, do not error
        let (_, capabilities, _, capabilitiesError) = await remoteInterface.currentCapabilities(
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: itemTemplate.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )
        guard capabilitiesError == .success,
              let capabilities,
              capabilities.files?.locking != nil
        else {
            logger.info(
                """
                Received nil capabilities data.
                    Received error: \(capabilitiesError.errorDescription)
                    Capabilities nil: \(capabilities == nil ? "YES" : "NO")
                    (if capabilities are not nil the server may just not have files_lock enabled).
                    Will not proceed with locking for \(itemTemplate.filename)
                """
            )
            return (nil, nil)
        }

        logger.info(
            """
            Item to create:
                \(itemTemplate.filename)
                is a lock file. Will handle by remotely locking the target file.
            """
        )
        guard let targetFileName = originalFileName(
            fromLockFileName: itemTemplate.filename
        ) else {
            logger.error(
                """
                Could not get original filename from lock file filename
                    \(itemTemplate.filename)
                    so will not lock target file.
                """
            )
            return (nil, nil)
        }
        let targetFileRemotePath = parentItemRemotePath + "/" + targetFileName
        let (_, _, error) = await remoteInterface.setLockStateForFile( // TODO: NOT WORKING
            remotePath: targetFileRemotePath,
            lock: true,
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: itemTemplate.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )
        if error != .success {
            logger.error(
                """
                Failed to lock target file \(targetFileName)
                    for lock file: \(itemTemplate.filename)
                    received error: \(error.errorDescription)
                """
            )
        } else {
            logger.info("Locked file at: \(targetFileRemotePath)")
        }

        let metadata = SendableItemMetadata(
            ocId: itemTemplate.itemIdentifier.rawValue,
            account: account.ncKitAccount,
            classFile: "lock", // Indicates this metadata is for a locked file
            contentType: itemTemplate.contentType?.preferredMIMEType ?? "",
            creationDate: itemTemplate.creationDate as? Date ?? Date(),
            date: Date(),
            directory: false,
            e2eEncrypted: false,
            etag: "",
            fileId: itemTemplate.itemIdentifier.rawValue,
            fileName: itemTemplate.filename,
            fileNameView: itemTemplate.filename,
            hasPreview: false,
            iconName: "lockIcon", // Custom icon for locked items
            mountType: "",
            ownerId: account.id,
            ownerDisplayName: "",
            path: parentItemRemotePath + "/" + targetFileName,
            serverUrl: parentItemRemotePath,
            size: 0,
            status: Status.normal.rawValue,
            downloaded: true,
            uploaded: false,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id
        )
        dbManager.addItemMetadata(metadata)

        progress.completedUnitCount = 1
        var returnError = error.fileProviderError // No need to handle problem cases, no upload here
        if #available(macOS 13.0, *), error == .success {
            returnError = NSFileProviderError(.excludedFromSync)
        }

        return (
            Item(
                metadata: metadata,
                parentItemIdentifier: parentItemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                remoteSupportsTrash: await remoteInterface.supportsTrash(account: account),
                log: log
            ),
            returnError
        )
    }

    func modifyLockFile(
        itemTarget: NSFileProviderItem,
        baseVersion: NSFileProviderItemVersion = NSFileProviderItemVersion(),
        changedFields: NSFileProviderItemFields,
        contents newContents: URL?,
        options: NSFileProviderModifyItemOptions = [],
        request: NSFileProviderRequest = NSFileProviderRequest(),
        ignoredFiles: IgnoredFilesMatcher? = nil,
        domain: NSFileProviderDomain? = nil,
        forcedChunkSize: Int? = nil,
        progress: Progress = .init(),
        dbManager: FilesDatabaseManager
    ) async -> (Item?, Error?) {
        logger.info(
            """
            System requested modification of lockfile \(self.filename)
                Marking as complete without syncing to server.
            """
        )
        assert(isLockFileName(filename), "Should not handle non-lock files here.")

        guard let modifiedItem = await modifyUnuploaded(
            itemTarget: itemTarget,
            baseVersion: baseVersion,
            changedFields: changedFields,
            contents: newContents,
            options: options,
            request: request,
            ignoredFiles: ignoredFiles,
            domain: domain,
            forcedChunkSize: forcedChunkSize,
            progress: progress,
            dbManager: dbManager
        ) else {
            logger.info(
                "Cannot modify lock file: \(self.filename) as received a nil modified item"
            )
            return (nil, NSFileProviderError(.cannotSynchronize))
        }

        if !isLockFileName(modifiedItem.filename) {
            logger.info(
                """
                After modification, lock file: \(self.filename) is no longer a lock file
                    (it is now named: \(modifiedItem.filename))
                    Will proceed with creating item on server (if possible).
                """
            )
            return await modifiedItem.createUnuploaded(
                itemTarget: itemTarget,
                baseVersion: baseVersion,
                changedFields: changedFields,
                contents: newContents,
                options: options,
                request: request,
                ignoredFiles: ignoredFiles,
                domain: domain,
                forcedChunkSize: forcedChunkSize,
                progress: progress,
                dbManager: dbManager
            )
        }

        var returnError: Error? = nil
        if #available(macOS 13.0, *) {
            returnError = NSFileProviderError(.excludedFromSync)
        }
        return (modifiedItem, returnError)
    }

    func deleteLockFile(
        domain: NSFileProviderDomain? = nil, dbManager: FilesDatabaseManager
    ) async -> Error? {
        let (_, capabilities, _, capabilitiesError) = await remoteInterface.currentCapabilities(
            account: account,
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
        guard capabilitiesError == .success,
              let capabilities,
              capabilities.files?.locking != nil
        else {
            logger.info(
                """
                Received nil capabilities data.
                    Received error: \(capabilitiesError.errorDescription)
                    Capabilities nil: \(capabilities == nil ? "YES" : "NO")
                    (if capabilities are not nil the server may just not have files_lock enabled).
                    Will not proceed with unlocking for \(self.filename)
                """
            )
            return nil
        }

        dbManager.deleteItemMetadata(ocId: metadata.ocId)

        guard let originalFileName = originalFileName(
            fromLockFileName: metadata.fileName
        ) else {
            logger.error(
                """
                Could not get original filename from lock file filename
                    \(self.metadata.fileName)
                    so will not unlock target file.
                """
            )
            return nil
        }
        let originalFileServerFileNameUrl = metadata.serverUrl + "/" + originalFileName
        let (_, _, error) = await remoteInterface.setLockStateForFile( // TODO: NOT WORKING
            remotePath: originalFileServerFileNameUrl,
            lock: false,
            account: account,
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
        guard error == .success else {
            logger.error(
                """
                Could not unlock item for \(self.filename)...
                    at \(originalFileServerFileNameUrl)...
                    received error: \(error.errorCode)
                    \(error.errorDescription)
                """
            )
            return error.fileProviderError(
                handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
            )
        }
        logger.info(
            """
            Successfully unlocked item for: \(self.filename)...
                at: \(originalFileServerFileNameUrl)
            """
        )
        return nil
    }
}

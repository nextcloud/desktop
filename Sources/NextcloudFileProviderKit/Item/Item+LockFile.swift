//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudKit
import NextcloudCapabilitiesKit

extension Item {
    ///
    /// Shared capability assertion before dispatching (un)lock requests to the server.
    ///
    private static func assertRequiredCapabilities(domain: NSFileProviderDomain?, itemIdentifier: NSFileProviderItemIdentifier, account: Account, remoteInterface: RemoteInterface, logger: FileProviderLogger) async -> Bool {
        let (_, capabilities, _, capabilitiesError) = await remoteInterface.currentCapabilities(
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard capabilitiesError == .success else {
            logger.error("Request for capability assertion failed!")
            return false
        }

        guard let capabilities else {
            logger.error("Capabilities to assert are nil!")
            return false
        }

        guard capabilities.files?.locking != nil else {
            logger.error("Capability assertion failed because file locks are not supported!")
            return false
        }

        return true
    }

    ///
    /// Create a lock file in the local file provider extension database which is not synchronized to the server, if the server supports file locking.
    /// The lock file itself is not uploaded and no error is reported intentionally.
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

        guard await assertRequiredCapabilities(domain: domain, itemIdentifier: itemTemplate.itemIdentifier, account: account, remoteInterface: remoteInterface, logger: logger) else {
            return (nil, nil)
        }

        logger.info("Item to create is a lock file. Will attempt to lock the associated file on the server.", [.name: itemTemplate.filename])

        guard let targetFileName = originalFileName(fromLockFileName: itemTemplate.filename) else {
            logger.error("Will not lock the target file because it could not be determined based on the lock file name.", [.name: itemTemplate.filename])
            return (nil, nil)
        }

        let targetFileRemotePath = parentItemRemotePath + "/" + targetFileName

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
        var errorToReturn: Error?

        do {
            let lock = try await remoteInterface.lockUnlockFile(serverUrlFileName: targetFileRemotePath, type: .token, shouldLock: true, account: account, options: .init(), taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: itemTemplate.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            })

            if let lock {
                logger.info("Locked file and received lock.", [.name: targetFileName, .lock: lock])
            } else {
                logger.info("Locked file but did not receive lock information.", [.name: targetFileName])
            }

            if #available(macOS 13.0, *) {
                errorToReturn = NSFileProviderError(.excludedFromSync)
            }
        } catch {
            logger.error("Failed to lock file \"\(targetFileName)\" which has lock file \"\(itemTemplate.filename)\".", [.error: error])

            if let nkError = error as? NKError {
                // Attempt to map a possible NKError to an NSFileProviderError.
                errorToReturn = nkError.fileProviderError
            } else {
                // Return the error as it is.
                errorToReturn = error
            }
        }

        progress.completedUnitCount = 1

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
            errorToReturn
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
        logger.info("System requested modification of lock file. Marking as complete without syncing to server.", [.name: self.filename])

        if isLockFileName(filename) == false {
            logger.fault("Should not handle non-lock files here.", [.name: filename])
        }

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
            logger.info("Cannot modify lock file because received a nil modified item.", [.name: self.filename])
            return (nil, NSFileProviderError(.cannotSynchronize))
        }

        if !isLockFileName(modifiedItem.filename) {
            logger.info("After modification, lock file: \(self.filename) is no longer a lock file (it is now named: \(modifiedItem.filename)) Will proceed with creating item on server (if possible).")

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

    func deleteLockFile(domain: NSFileProviderDomain? = nil, dbManager: FilesDatabaseManager) async -> Error? {
        guard await Self.assertRequiredCapabilities(domain: domain, itemIdentifier: self.itemIdentifier, account: account, remoteInterface: remoteInterface, logger: logger) else {
            return nil
        }

        dbManager.deleteItemMetadata(ocId: metadata.ocId)

        guard let originalFileName = originalFileName(fromLockFileName: metadata.fileName) else {
            logger.error("Could not get original filename from lock file filename so will not unlock target file.", [.name: self.metadata.fileName])
            return nil
        }

        let originalFileServerFileNameUrl = metadata.serverUrl + "/" + originalFileName

        do {
            let lock = try await remoteInterface.lockUnlockFile(
                serverUrlFileName: originalFileServerFileNameUrl,
                type: .token,
                shouldLock: false,
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

            if let lock {
                logger.info("Unlocked file and received lock.", [.name: originalFileName, .lock: lock])
            } else {
                logger.info("Unlocked file but did not receive lock information.", [.name: originalFileName])
            }

        } catch {
            logger.error("Could not unlock item.", [.name: self.filename, .error: error])

            if let error = error as? NKError {
                return error.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier)
            }
        }

        return nil
    }
}

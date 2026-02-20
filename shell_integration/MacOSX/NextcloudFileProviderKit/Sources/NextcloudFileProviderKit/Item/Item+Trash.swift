//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

extension Item {
    /// Note: When handling trashing, the server handles filename conflicts for us
    static func trash(
        _ modifiedItem: Item,
        account: Account,
        dbManager: FilesDatabaseManager,
        domain: NSFileProviderDomain?,
        log: any FileProviderLogging
    ) async -> (Item, Error?) {
        let logger = FileProviderLogger(category: "Item", log: log)

        let deleteError = await modifiedItem.delete(trashing: true, domain: domain, dbManager: dbManager)

        guard deleteError == nil else {
            logger.error("Error attempting to move item into trash.", [.name: modifiedItem.filename, .error: deleteError])
            return (modifiedItem, deleteError)
        }

        let ocId = modifiedItem.itemIdentifier.rawValue

        guard let dirtyMetadata = dbManager.itemMetadata(ocId: ocId) else {
            logger.error("Could not correctly process trashing results, dirty metadata not found.", [.item: ocId, .name: modifiedItem.filename])
            return (modifiedItem, NSFileProviderError(.cannotSynchronize))
        }

        let dirtyChildren = dbManager.childItems(directoryMetadata: dirtyMetadata)

        let dirtyItem = await Item(
            metadata: dirtyMetadata,
            parentItemIdentifier: .trashContainer,
            account: account,
            remoteInterface: modifiedItem.remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: modifiedItem.remoteInterface.supportsTrash(account: account),
            log: log
        )

        // The server may have renamed the trashed file so we need to scan the entire trash
        let (_, files, _, error) = await modifiedItem.remoteInterface.listingTrashAsync(
            filename: nil,
            showHiddenFiles: true,
            account: account.ncKitAccount,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: modifiedItem.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard error == .success else {
            logger.error("Received error from post-trashing remote scan.", [.error: error])

            return (dirtyItem, error.fileProviderError)
        }

        guard let targetItemNKTrash = files?.first(
            // It seems the server likes to return a fileId as the ocId for trash files, so let's
            // check for the fileId too
            where: { $0.ocId == modifiedItem.metadata.ocId ||
                $0.fileId == modifiedItem.metadata.fileId
            }
        )
        else {
            logger.error("Did not find trashed item in trash, asking for a rescan.", [.item: modifiedItem])
            return (dirtyItem, NSFileProviderError(.unsyncedEdits))
        }

        var postDeleteMetadata = targetItemNKTrash.toItemMetadata(account: account)
        postDeleteMetadata.ocId = modifiedItem.itemIdentifier.rawValue
        dbManager.addItemMetadata(postDeleteMetadata)

        let postDeleteItem = await Item(
            metadata: postDeleteMetadata,
            parentItemIdentifier: .trashContainer,
            account: account,
            remoteInterface: modifiedItem.remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: modifiedItem.remoteInterface.supportsTrash(account: account),
            log: log
        )

        // Now we can directly update info on the child items
        var (_, childFiles, _, childError) = await modifiedItem.remoteInterface.enumerate(
            remotePath: postDeleteMetadata.serverUrl + "/" + postDeleteMetadata.fileName,
            depth: EnumerateDepth.targetAndAllChildren, // Just do it in one go
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: modifiedItem.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard error == .success else {
            logger.error("Received error or files from post-trashing child items remote scan.", [.error: error])
            return (postDeleteItem, childError.fileProviderError)
        }

        // Update state of child files
        childFiles.removeFirst() // This is the target path, already scanned

        for file in childFiles {
            var metadata = file.toItemMetadata()

            guard let original = dirtyChildren
                .filter({ $0.ocId == metadata.ocId || $0.fileId == metadata.fileId })
                .first
            else {
                logger.info("Skipping post-trash child item metadata. Could not find matching existing item in database, cannot do ocId correction.", [.name: metadata.fileName])
                continue
            }

            metadata.ocId = original.ocId // Give original id back
            dbManager.addItemMetadata(metadata)
            logger.info("The previous addition was a post-trash child item metadata.")
        }

        return (postDeleteItem, nil)
    }

    /// Note: When restoring from the trash, the server handles filename conflicts for us
    static func restoreFromTrash(
        _ modifiedItem: Item,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        domain _: NSFileProviderDomain?,
        log: any FileProviderLogging
    ) async -> (Item, Error?) {
        let logger = FileProviderLogger(category: "Item", log: log)

        func finaliseRestore(target: NKFile) async -> (Item, Error?) {
            let restoredItemMetadata = target.toItemMetadata()

            guard let parentItemIdentifier = await dbManager.parentItemIdentifierWithRemoteFallback(
                fromMetadata: restoredItemMetadata,
                remoteInterface: remoteInterface,
                account: account
            ) else {
                logger.error("Could not find parent item identifier for \(originalLocation)")
                return (modifiedItem, NSFileProviderError(.cannotSynchronize))
            }

            if restoredItemMetadata.directory {
                _ = dbManager.renameDirectoryAndPropagateToChildren(
                    ocId: restoredItemMetadata.ocId,
                    newServerUrl: restoredItemMetadata.serverUrl,
                    newFileName: restoredItemMetadata.fileName
                )
            }

            dbManager.addItemMetadata(restoredItemMetadata)

            return await (Item(
                metadata: restoredItemMetadata,
                parentItemIdentifier: parentItemIdentifier,
                account: account,
                remoteInterface: modifiedItem.remoteInterface,
                dbManager: dbManager,
                remoteSupportsTrash: modifiedItem.remoteInterface.supportsTrash(account: account),
                log: log
            ), nil)
        }

        let (_, _, restoreError) = await modifiedItem.remoteInterface.restoreFromTrash(
            filename: modifiedItem.metadata.fileName,
            account: account,
            options: .init(),
            taskHandler: { _ in }
        )

        guard restoreError == .success else {
            logger.error("Could not restore item from trash.", [.name: modifiedItem.filename, .error: restoreError.errorDescription])
            return (modifiedItem, restoreError.fileProviderError)
        }

        guard modifiedItem.metadata.trashbinOriginalLocation != "" else {
            logger.error("Could not scan restored item. The trashed file's original location is invalid.", [.name: modifiedItem.filename])
            return (modifiedItem, NSFileProviderError(.unsyncedEdits))
        }

        let originalLocation = account.davFilesUrl + "/" + modifiedItem.metadata.trashbinOriginalLocation

        let (_, files, _, enumerateError) = await modifiedItem.remoteInterface.enumerate(
            remotePath: originalLocation,
            depth: .target,
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            account: account,
            options: .init(),
            taskHandler: { _ in }
        )

        guard enumerateError == .success, !files.isEmpty, let target = files.first else {
            logger.error(
                """
                Could not scan restored state of file \(originalLocation)
                Received error: \(enumerateError.errorDescription)
                Files: \(files.count)
                """
            )

            return (modifiedItem, NSFileProviderError(.unsyncedEdits))
        }

        guard target.ocId == modifiedItem.itemIdentifier.rawValue else {
            logger.info("Restored item at location does not match name (it is likely that when restoring from the trash, there was another identical item).", [.name: modifiedItem.filename, .url: originalLocation])

            guard let finalSlashIndex = originalLocation.lastIndex(of: "/") else {
                return (modifiedItem, NSFileProviderError(.cannotSynchronize))
            }

            var parentDirectoryRemotePath = originalLocation
            parentDirectoryRemotePath.removeSubrange(finalSlashIndex ..< originalLocation.endIndex)

            logger.info("Scanning parent folder at \(parentDirectoryRemotePath) for current state of item restored from trash.")

            let (_, files, _, folderScanError) = await modifiedItem.remoteInterface.enumerate(
                remotePath: parentDirectoryRemotePath,
                depth: .targetAndDirectChildren,
                showHiddenFiles: true,
                includeHiddenFiles: [],
                requestBody: nil,
                account: account,
                options: .init(),
                taskHandler: { _ in }
            )

            guard folderScanError == .success else {
                logger.error(
                    """
                    Scanning parent folder at \(parentDirectoryRemotePath)
                    returned error: \(folderScanError.errorDescription)
                    """
                )
                return (modifiedItem, NSFileProviderError(.cannotSynchronize))
            }

            guard let actualTarget = files.first(
                where: { $0.ocId == modifiedItem.itemIdentifier.rawValue }
            ) else {
                logger.error(
                    """
                    Scanning parent folder at \(parentDirectoryRemotePath)
                    finished successfully but the target item restored from trash not found.
                    """
                )
                return (modifiedItem, NSFileProviderError(.cannotSynchronize))
            }

            return await finaliseRestore(target: actualTarget)
        }

        return await finaliseRestore(target: target)
    }
}

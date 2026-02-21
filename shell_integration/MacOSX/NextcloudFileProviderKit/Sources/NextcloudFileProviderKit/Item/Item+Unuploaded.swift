//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider

extension Item {
    /// Creates a file that was previously unuploaded (e.g. a previously ignored/lock file) on server
    func createUnuploaded(
        itemTarget: NSFileProviderItem,
        baseVersion _: NSFileProviderItemVersion = NSFileProviderItemVersion(),
        changedFields _: NSFileProviderItemFields,
        contents newContents: URL?,
        options _: NSFileProviderModifyItemOptions = [],
        request _: NSFileProviderRequest = NSFileProviderRequest(),
        ignoredFiles: IgnoredFilesMatcher? = nil,
        domain: NSFileProviderDomain? = nil,
        forcedChunkSize: Int? = nil,
        progress: Progress = .init(),
        dbManager: FilesDatabaseManager
    ) async -> (Item?, Error?) {
        guard newContents != nil || domain != nil else {
            logger.error(
                """
                Unable to upload modified item that was previously unuploaded.
                    filename: \(filename)
                    either the domain is nil, the provided contents are nil, or both.
                """
            )
            return (nil, NSFileProviderError(.cannotSynchronize))
        }
        let modifiedItem = self
        var contentsLocation = newContents
        if contentsLocation == nil {
            // TODO: Find a way to test me
            assert(domain != nil)
            guard let domain, let localUrl = await localUrlForContents(domain: domain) else {
                logger.error(
                    """
                    Unable to upload modified item that was previously unuploaded.
                        filename: \(modifiedItem.filename)
                        local url for contents could not be acquired.
                    """
                )
                return (nil, NSFileProviderError(.cannotSynchronize))
            }
            contentsLocation = localUrl
        }
        return await Self.create(
            basedOn: itemTarget,
            contents: contentsLocation,
            domain: domain,
            account: account,
            remoteInterface: remoteInterface,
            ignoredFiles: ignoredFiles,
            forcedChunkSize: forcedChunkSize,
            progress: progress,
            dbManager: dbManager,
            log: logger.log
        )
    }

    /// Just modifies metadata
    func modifyUnuploaded(
        itemTarget: NSFileProviderItem,
        baseVersion _: NSFileProviderItemVersion = NSFileProviderItemVersion(),
        changedFields: NSFileProviderItemFields,
        contents newContents: URL?,
        options _: NSFileProviderModifyItemOptions = [],
        request _: NSFileProviderRequest = NSFileProviderRequest(),
        ignoredFiles _: IgnoredFilesMatcher? = nil,
        domain _: NSFileProviderDomain? = nil,
        forcedChunkSize _: Int? = nil,
        progress _: Progress = .init(),
        dbManager: FilesDatabaseManager
    ) async -> Item? {
        var modifiedParentItemIdentifier = parentItemIdentifier
        var modifiedMetadata = metadata

        if changedFields.contains(.filename) {
            modifiedMetadata.fileName = itemTarget.filename
            if !isLockFileName(modifiedMetadata.fileName) {
                modifiedMetadata.classFile = ""
                // Do the actual upload at the end, not yet
            }
        }
        if changedFields.contains(.contents),
           let newSize = try? newContents?.resourceValues(forKeys: [.fileSizeKey]).fileSize
        {
            modifiedMetadata.size = Int64(newSize)
        }
        if changedFields.contains(.parentItemIdentifier) {
            guard let parentMetadata = dbManager.itemMetadata(
                ocId: itemTarget.parentItemIdentifier.rawValue
            ) else {
                logger.error(
                    """
                    Unable to find new parent item identifier during unuploaded item modification.
                        Filename: \(filename)
                    """
                )
                return nil
            }
            modifiedMetadata.serverUrl = parentMetadata.serverUrl + "/" + parentMetadata.fileName
            modifiedParentItemIdentifier = .init(parentMetadata.ocId)
        }
        if changedFields.contains(.creationDate),
           let newCreationDate = itemTarget.creationDate,
           let newCreationDate
        {
            modifiedMetadata.creationDate = newCreationDate
        }
        if changedFields.contains(.contentModificationDate),
           let newModificationDate = itemTarget.contentModificationDate,
           let newModificationDate
        {
            modifiedMetadata.date = newModificationDate
        }

        let displayFileActions = await Item.typeHasApplicableContextMenuItems(account: account, remoteInterface: remoteInterface, candidate: modifiedMetadata.contentType)

        return await Item(
            metadata: modifiedMetadata,
            parentItemIdentifier: modifiedParentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: displayFileActions,
            remoteSupportsTrash: remoteInterface.supportsTrash(account: account),
            log: logger.log
        )
    }
}

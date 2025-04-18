//
//  Item+ModifyLockFile.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 17/4/25.
//

import FileProvider

extension Item {
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
        Self.logger.info(
            """
            System requested modification of lockfile \(self.filename, privacy: .public)
                Marking as complete without syncing to server.
            """
        )
        assert(isLockFileName(filename), "Should not handle non-lock files here.")

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
                Self.logger.error(
                    """
                    Unable to find new parent item identifier during lock file modification.
                        Lock file: \(self.filename, privacy: .public)
                    """
                )
                return (nil, NSFileProviderError(.cannotSynchronize))
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

        let modifiedItem = Item(
            metadata: modifiedMetadata,
            parentItemIdentifier: modifiedParentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )

        if !isLockFileName(modifiedItem.filename) {
            Self.logger.info(
                """
                After modification, lock file: \(self.filename) is no longer a lock file
                    (it is now named: \(modifiedItem.filename))
                    Will proceed with creating item on server (if possible).
                """
            )
            return await modifiedItem.modifyUnuploaded(
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

        return (modifiedItem, nil)
    }
}

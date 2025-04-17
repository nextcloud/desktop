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
        domain: NSFileProviderDomain? = nil,
        progress: Progress = .init(),
        dbManager: FilesDatabaseManager
    ) -> (Item?, Error?) {
        Self.logger.info(
            """
            System requested modification of lockfile \(self.filename, privacy: .public)
                Marking as complete without syncing to server.
            """
        )

        var modifiedParentItemIdentifier = parentItemIdentifier
        var modifiedMetadata = metadata

        if changedFields.contains(.filename) {
            modifiedMetadata.fileName = itemTarget.filename
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
        return (modifiedItem, nil)
    }
}

//
//  Item+ModifyUnuploaded.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 17/4/25.
//

import FileProvider

extension Item {
    // Just modifies metadata
    func modifyUnuploaded(
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
    ) -> Item? {
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
                    Unable to find new parent item identifier during unuploaded item modification.
                        Filename: \(self.filename, privacy: .public)
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

        return Item(
            metadata: modifiedMetadata,
            parentItemIdentifier: modifiedParentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )
    }
}

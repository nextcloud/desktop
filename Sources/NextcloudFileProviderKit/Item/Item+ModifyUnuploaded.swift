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

    // Creates a file that was previously unuploaded (e.g. a previously ignored/lock file) on server
    func createUnuploadedRemotely(
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
        guard newContents != nil || domain != nil else {
            Self.logger.error(
                """
                Unable to upload modified item that was previously unuploaded.
                    filename: \(self.filename, privacy: .public)
                    either the domain is nil, the provided contents are nil, or both.
                """
            )
            return (nil, NSFileProviderError(.cannotSynchronize))
        }
        let modifiedItem = self
        var contentsLocation = newContents
        if contentsLocation == nil {
            assert(domain != nil)
            guard let domain, let localUrl = await localUrlForContents(domain: domain) else {
                Self.logger.error(
                    """
                    Unable to upload modified item that was previously unuploaded.
                        filename: \(modifiedItem.filename, privacy: .public)
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
            dbManager: dbManager
        )
    }
}

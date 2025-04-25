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

        guard let modifiedItem = modifyUnuploaded(
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
            Self.logger.info(
                "Cannot modify lock file: \(self.filename) as received a nil modified item"
            )
            return (nil, NSFileProviderError(.cannotSynchronize))
        }

        if !isLockFileName(modifiedItem.filename) {
            Self.logger.info(
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
}

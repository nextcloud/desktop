//
//  Item+ModifyUnuploaded.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 17/4/25.
//

import FileProvider

extension Item {
    // Creates a file that was previously unuploaded (e.g.a  previously ignored/lock file) on server
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
                Unable to upload modified item that was previously ignored.
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
                    Unable to upload modified item that was previously ignored.
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

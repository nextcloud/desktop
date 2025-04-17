//
//  Item+ModifyUnuploaded.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 17/4/25.
//

import FileProvider

extension Item {
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
            assert(domain != nil, "The domain should not be nil!")
            guard let manager = NSFileProviderManager(for: domain!),
                  let fileUrl = try? await manager.getUserVisibleURL(
                    for: modifiedItem.itemIdentifier
                  )
            else {
                Self.logger.error(
                    """
                    Unable to upload modified item that was previously ignored.
                        filename: \(modifiedItem.filename, privacy: .public)
                        Unable to get a file provider manager for the given domain, or item URL
                    """
                )
                return (nil, NSFileProviderError(.cannotSynchronize))
            }
            let fm = FileManager.default
            let tempLocation = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
            let coordinator = NSFileCoordinator()
            var readData: Data?
            coordinator.coordinate(readingItemAt: fileUrl, options: [], error: nil) { readURL in
                readData = try? Data(contentsOf: readURL)
            }
            guard let readData else {
                Self.logger.error(
                    """
                    Unable to upload modified item that was previously ignored.
                        filename: \(modifiedItem.filename, privacy: .public)
                        Unable to get ignored file item data from URL
                    """
                )
                return (nil, NSFileProviderError(.cannotSynchronize))
            }
            do {
                try readData.write(to: tempLocation)
            } catch let error {
                Self.logger.error(
                    """
                    Unable to upload modified item that was previously ignored.
                        filename: \(modifiedItem.filename, privacy: .public)
                        Unable to write ignored file item contents to temp location.
                        error: \(error.localizedDescription, privacy: .public)
                    """
                )
            }
            contentsLocation = tempLocation
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

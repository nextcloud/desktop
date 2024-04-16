//
//  Item+Fetch.swift
//
//
//  Created by Claudio Cambra on 16/4/24.
//

import FileProvider
import Foundation
import NextcloudKit
import OSLog

public extension Item {
    
    func fetchContents(
        domain: NSFileProviderDomain, progress: Progress
    ) async -> (URL?, Item?, Error?) {
        let ocId = itemIdentifier.rawValue
        let serverUrlFileName = metadata.serverUrl + "/" + metadata.fileName

        Self.logger.debug(
            """
            Fetching file with name \(self.metadata.fileName, privacy: .public)
            at URL: \(serverUrlFileName, privacy: .public)
            """
        )

        // TODO: Handle folders nicely
        var fileNameLocalPath: URL?
        do {
            fileNameLocalPath = try localPathForNCFile(
                ocId: metadata.ocId, fileNameView: metadata.fileNameView, domain: domain
            )
        } catch {
            Self.logger.error(
                """
                Could not find local path for file \(self.metadata.fileName, privacy: .public),
                received error: \(error.localizedDescription, privacy: .public)
                """
            )
            return (nil, nil, NSFileProviderError(.cannotSynchronize))
        }

        guard let fileNameLocalPath = fileNameLocalPath else {
            Self.logger.error(
                """
                Could not find local path for file \(self.metadata.fileName, privacy: .public)
                """
            )
            return (nil, nil, NSFileProviderError(.cannotSynchronize))
        }

        let dbManager = FilesDatabaseManager.shared
        let updatedMetadata = await withCheckedContinuation { continuation in
            dbManager.setStatusForItemMetadata(metadata, status: .downloading) { updatedMeta in
                continuation.resume(returning: updatedMeta)
            }
        }

        guard let updatedMetadata = updatedMetadata else {
            Self.logger.error(
                """
                Could not acquire updated metadata of item \(ocId, privacy: .public),
                unable to update item status to downloading
                """
            )
            return (nil, nil, NSFileProviderError(.noSuchItem))
        }

        let (etag, date, error) = await withCheckedContinuation { continuation in
            self.ncKit.download(
                serverUrlFileName: serverUrlFileName,
                fileNameLocalPath: fileNameLocalPath.path,
                requestHandler: {  progress.setHandlersFromAfRequest($0) },
                taskHandler: { task in
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                },
                progressHandler: { downloadProgress in
                    downloadProgress.copyCurrentStateToProgress(progress)
                }
            ) { _, etag, date, _, _, _, error in
                continuation.resume(returning: (etag, date, error))
            }
        }

        if let fpError = error.fileProviderError {
            Self.logger.error(
                """
                Could not acquire contents of item with identifier: \(ocId, privacy: .public)
                and fileName: \(updatedMetadata.fileName, privacy: .public)
                at \(serverUrlFileName, privacy: .public)
                error: \(fpError.localizedDescription, privacy: .public)
                """
            )

            updatedMetadata.status = ItemMetadata.Status.downloadError.rawValue
            updatedMetadata.sessionError = error.errorDescription
            dbManager.addItemMetadata(updatedMetadata)
            return (nil, nil, fpError)
        }

        Self.logger.debug(
            """
            Acquired contents of item with identifier: \(ocId, privacy: .public)
            and filename: \(updatedMetadata.fileName, privacy: .public)
            """
        )

        updatedMetadata.status = ItemMetadata.Status.normal.rawValue
        updatedMetadata.sessionError = ""
        updatedMetadata.date = (date ?? NSDate()) as Date
        updatedMetadata.etag = etag ?? ""

        dbManager.addLocalFileMetadataFromItemMetadata(updatedMetadata)
        dbManager.addItemMetadata(updatedMetadata)

        guard let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(
            updatedMetadata
        ) else { return (nil, nil, NSFileProviderError(.noSuchItem)) }

        let fpItem = Item(
            metadata: updatedMetadata,
            parentItemIdentifier: parentItemIdentifier,
            ncKit: self.ncKit
        )

        return (fileNameLocalPath, fpItem, nil)
    }
}

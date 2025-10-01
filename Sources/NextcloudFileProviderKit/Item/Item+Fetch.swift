//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudKit

public extension Item {
    private func fetchDirectoryContents(
        itemIdentifier: NSFileProviderItemIdentifier,
        directoryLocalPath: String,
        directoryRemotePath: String,
        domain: NSFileProviderDomain?,
        progress: Progress
    ) async throws {
        progress.totalUnitCount = 1 // Add 1 for final procedures

        // Download *everything* within this directory. What we do:
        // 1. Enumerate the contents of the directory
        // 2. Download everything within this directory
        // 3. Detect child directories
        // 4. Repeat 1 -> 3 for each child directory
        var remoteDirectoryPaths = [directoryRemotePath]
        while !remoteDirectoryPaths.isEmpty {
            let remoteDirectoryPath = remoteDirectoryPaths.removeFirst()
            let (metadatas, _, _, _, _, readError) = await Enumerator.readServerUrl(
                remoteDirectoryPath,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                log: logger.log
            )

            if let readError, readError != .success {
                logger.error(
                    """
                    Could not enumerate directory contents for
                    \(self.metadata.fileName)
                    at \(remoteDirectoryPath)
                    error: \(readError.errorCode)
                    \(readError.errorDescription)
                    """
                )
                throw readError.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                ) ??  NSFileProviderError(.cannotSynchronize)
            }

            guard var metadatas else {
                logger.error(
                    """
                    Could not fetch directory contents for
                        \(self.metadata.fileName)
                        at \(remoteDirectoryPath), received nil metadatas
                    """
                )
                throw NSFileProviderError(.cannotSynchronize)
            }

            if !metadatas.isEmpty {
                metadatas.removeFirst() // Remove the dir itself
            }
            progress.totalUnitCount += Int64(metadatas.count)

            for var metadata in metadatas {
                let remotePath = metadata.serverUrl + "/" + metadata.fileName
                let relativePath =
                    remotePath.replacingOccurrences(of: directoryRemotePath, with: "")
                let childLocalPath = directoryLocalPath + relativePath

                if metadata.directory {
                    remoteDirectoryPaths.append(remotePath)
                    try FileManager.default.createDirectory(
                        at: URL(fileURLWithPath: childLocalPath),
                        withIntermediateDirectories: true,
                        attributes: nil
                    )
                } else {
                    let (_, _, _, _, _, _, error) = await remoteInterface.download(
                        remotePath: remotePath,
                        localPath: childLocalPath,
                        account: account,
                        options: .init(),
                        requestHandler: { progress.setHandlersFromAfRequest($0) },
                        taskHandler: { task in
                            if let domain {
                                NSFileProviderManager(for: domain)?.register(
                                    task,
                                    forItemWithIdentifier: 
                                        NSFileProviderItemIdentifier(metadata.ocId),
                                    completionHandler: { _ in }
                                )
                            }
                        },
                        progressHandler: { _ in }
                    )

                    guard error == .success else {
                        logger.error(
                        """
                        Could not acquire contents of item: \(metadata.fileName)
                        at \(remotePath)
                        error: \(error.errorCode)
                        \(error.errorDescription)
                        """
                        )
                        metadata.status = Status.downloadError.rawValue
                        metadata.sessionError = error.errorDescription
                        dbManager.addItemMetadata(metadata)
                        throw error.fileProviderError(
                            handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                        ) ??  NSFileProviderError(.cannotSynchronize)
                    }
                }

                metadata.status = Status.normal.rawValue
                metadata.downloaded = true
                // HACK: We were previously failing to correctly set the uploaded state to true for
                // enumerated items. Fix it now to ensure we do not show "waiting for upload" when
                // having downloaded incorrectly enumerated files
                metadata.uploaded = true
                metadata.sessionError = ""
                dbManager.addItemMetadata(metadata)

                progress.completedUnitCount += 1
            }
        }

        progress.completedUnitCount += 1 // Finish off
    }

    func fetchContents(
        domain: NSFileProviderDomain? = nil,
        progress: Progress = .init(),
        dbManager: FilesDatabaseManager
    ) async -> (URL?, Item?, Error?) {
        let ocId = itemIdentifier.rawValue
        guard metadata.classFile != "lock", !isLockFileName(filename) else {
            logger.info(
                """
                System requested fetch of lock file \(self.filename)
                    will just provide local contents URL if possible.
                """
            )
            if let domain, let localUrl = await localUrlForContents(domain: domain) {
                return (localUrl, self, nil)
            } else if #available(macOS 13.0, *) {
                logger.error("Could not get local contents URL for lock file, erroring")
                return (nil, self, NSFileProviderError(.excludedFromSync))
            } else {
                logger.error("Could not get local contents URL for lock file, nilling")
                return (nil, self, nil)
            }
        }

        let serverUrlFileName = metadata.serverUrl + "/" + metadata.fileName

        logger.debug(
            """
            Fetching item with name \(self.metadata.fileName)
                at URL: \(serverUrlFileName)
            """
        )

        let localPath = FileManager.default.temporaryDirectory.appendingPathComponent(metadata.ocId)
        guard var updatedMetadata = dbManager.setStatusForItemMetadata(metadata, status: .downloading) else {
            logger.error(
                """
                Could not acquire updated metadata of item \(ocId),
                unable to update item status to downloading
                """
            )
            return (
                nil,
                nil,
                NSError.fileProviderErrorForNonExistentItem(withIdentifier: self.itemIdentifier)
            )
        }

        let isDirectory = contentType.conforms(to: .directory)
        if isDirectory {
            logger.debug(
                """
                Item with identifier: \(ocId)
                and filename: \(updatedMetadata.fileName)
                is a directory, creating dir locally and fetching its contents
                """
            )

            do {
                try FileManager.default.createDirectory(
                    at: localPath,
                    withIntermediateDirectories: true,
                    attributes: nil
                )
            } catch let error {
                logger.error("Could not create directory for item.", [.name: updatedMetadata.fileName, .error: error, .url: localPath])

                updatedMetadata.status = Status.downloadError.rawValue
                updatedMetadata.sessionError = error.localizedDescription
                dbManager.addItemMetadata(updatedMetadata)
                return (nil, nil, error)
            }

            do {
                try await fetchDirectoryContents(
                    itemIdentifier: itemIdentifier,
                    directoryLocalPath: localPath.path,
                    directoryRemotePath: serverUrlFileName,
                    domain: domain,
                    progress: progress
                )
            } catch {
                logger.error("Could not fetch directory contents.", [.ocId: ocId, .error: error])

                updatedMetadata.status = Status.downloadError.rawValue
                updatedMetadata.sessionError = error.localizedDescription
                dbManager.addItemMetadata(updatedMetadata)
                return (nil, nil, error)
            }

        } else {
            let (_, _, _, _, _, _, error) = await remoteInterface.download(
                remotePath: serverUrlFileName,
                localPath: localPath.path,
                account: account,
                options: .init(),
                requestHandler: { _ in },
                taskHandler: { _ in },
                progressHandler: { _ in }
            )

            if error != .success {
                logger.error(
                    """
                    Could not acquire contents of item with identifier: \(ocId)
                        and fileName: \(updatedMetadata.fileName)
                        at \(serverUrlFileName)
                        error: \(error.errorCode)
                        \(error.errorDescription)
                    """
                )

                updatedMetadata.status = Status.downloadError.rawValue
                updatedMetadata.sessionError = error.errorDescription
                dbManager.addItemMetadata(updatedMetadata)
                return (nil, nil, error.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                ))
            }
        }

        logger.debug(
            """
            Acquired contents of item with identifier: \(ocId)
            and filename: \(updatedMetadata.fileName)
            """
        )

        updatedMetadata.status = Status.normal.rawValue
        updatedMetadata.downloaded = true
        // HACK: We were previously failing to correctly set the uploaded state to true for
        // enumerated items. Fix it now to ensure we do not show "waiting for upload" when
        // having downloaded incorrectly enumerated files
        updatedMetadata.uploaded = true
        updatedMetadata.sessionError = ""

        dbManager.addItemMetadata(updatedMetadata)

        guard let parentItemIdentifier = await dbManager.parentItemIdentifierWithRemoteFallback(
            fromMetadata: metadata,
            remoteInterface: remoteInterface,
            account: account
        ) else {
            logger.error(
                """
                Could not find parent item id for file \(self.metadata.fileName)
                """
            )
            return (
                nil,
                nil,
                NSError.fileProviderErrorForNonExistentItem(withIdentifier: self.itemIdentifier)
            )
        }

        let fpItem = Item(
            metadata: updatedMetadata,
            parentItemIdentifier: parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: await remoteInterface.supportsTrash(account: account),
            log: self.logger.log
        )

        return (localPath, fpItem, nil)
    }

    func fetchThumbnail(size: CGSize, domain: NSFileProviderDomain? = nil) async -> (Data?, Error?) {
        guard let thumbnailUrl = metadata.thumbnailUrl(size: size) else {
            logger.debug("Unknown thumbnail URL.", [.item: self.itemIdentifier, .name: self.filename])
            return (nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: self.itemIdentifier))
        }

        logger.debug("Fetching thumbnail.", [.name: self.filename, .url: thumbnailUrl])

        let (_, data, error) = await remoteInterface.downloadThumbnail(
            url: thumbnailUrl, account: account, options: .init(), taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        if error != .success {
            logger.error("Could not acquire thumbnail.", [.item: self.itemIdentifier, .name: self.filename, .url: thumbnailUrl, .error: error])
        }

        return (data, error.fileProviderError(
            handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
        ))
    }
}

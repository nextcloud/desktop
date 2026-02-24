//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
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
                logger.error("Could not enumerate directory contents.", [.name: metadata.fileName, .url: remoteDirectoryPath, .error: readError])

                throw readError.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                ) ?? NSFileProviderError(.cannotSynchronize)
            }

            guard var metadatas else {
                logger.error("Could not fetch directory contents.", [.name: metadata.fileName, .url: remoteDirectoryPath])
                throw NSFileProviderError(.cannotSynchronize)
            }

            if !metadatas.isEmpty {
                metadatas.removeFirst() // Remove the dir itself
            }
            progress.totalUnitCount += Int64(metadatas.count)

            for var metadata in metadatas {
                let remotePath = metadata.remotePath()
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
                    let identifier = NSFileProviderItemIdentifier(metadata.ocId)

                    let (_, _, _, _, _, _, error) = await remoteInterface.downloadAsync(
                        serverUrlFileName: remotePath,
                        fileNameLocalPath: childLocalPath,
                        account: account.ncKitAccount,
                        options: .init(),
                        requestHandler: { progress.setHandlersFromAfRequest($0) },
                        taskHandler: { task in
                            if let domain {
                                NSFileProviderManager(for: domain)?.register(task, forItemWithIdentifier: identifier, completionHandler: { _ in })
                            }
                        },
                        progressHandler: { _ in }
                    )

                    guard error == .success else {
                        logger.error("Could not acquire contents of item.", [.name: metadata.fileName, .url: remotePath, .error: error])
                        metadata.status = Status.downloadError.rawValue
                        metadata.sessionError = error.errorDescription
                        dbManager.addItemMetadata(metadata)
                        throw error.fileProviderError(
                            handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                        ) ?? NSFileProviderError(.cannotSynchronize)
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
            logger.info("System requested fetch of lock file, will just provide local contents URL if possible.", [.name: filename])

            if let domain, let localUrl = await localUrlForContents(domain: domain) {
                return (localUrl, self, nil)
            } else {
                logger.error("Could not get local content URL for lock file.")
                return (nil, self, NSFileProviderError(.excludedFromSync))
            }
        }

        let serverUrlFileName = metadata.remotePath()

        logger.debug("Fetching item.", [.name: metadata.fileName, .url: serverUrlFileName])

        let localPath = FileManager.default.temporaryDirectory.appendingPathComponent(metadata.ocId)
        guard var updatedMetadata = dbManager.setStatusForItemMetadata(metadata, status: .downloading) else {
            logger.error("Could not acquire updated metadata, unable to update item status to downloading.", [.item: itemIdentifier])

            return (
                nil,
                nil,
                NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier)
            )
        }

        let isDirectory = contentType.conforms(to: .directory)
        if isDirectory {
            logger.debug("is a directory, creating directory locally and fetching its contents.", [.item: ocId, .name: updatedMetadata.fileName])

            do {
                try FileManager.default.createDirectory(
                    at: localPath,
                    withIntermediateDirectories: true,
                    attributes: nil
                )
            } catch {
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
                logger.error("Could not fetch directory contents.", [.item: ocId, .error: error])

                updatedMetadata.status = Status.downloadError.rawValue
                updatedMetadata.sessionError = error.localizedDescription
                dbManager.addItemMetadata(updatedMetadata)
                return (nil, nil, error)
            }

        } else {
            let (_, _, _, _, _, _, error) = await remoteInterface.downloadAsync(
                serverUrlFileName: serverUrlFileName,
                fileNameLocalPath: localPath.path,
                account: account.ncKitAccount,
                options: .init(),
                requestHandler: { _ in },
                taskHandler: { _ in },
                progressHandler: { _ in }
            )

            if error != .success {
                logger.error("Could not acquire contents of item.", [.item: ocId, .name: updatedMetadata.fileName, .error: error])

                updatedMetadata.status = Status.downloadError.rawValue
                updatedMetadata.sessionError = error.errorDescription
                dbManager.addItemMetadata(updatedMetadata)
                return (nil, nil, error.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                ))
            }
        }

        logger.debug("Acquired contents of item.", [.item: ocId, .name: updatedMetadata.fileName])

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
            logger.error("Could not find parent item id for file.", [.name: metadata.fileName])

            return (nil, nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier))
        }

        let displayFileActions = await Item.typeHasApplicableContextMenuItems(account: account, remoteInterface: remoteInterface, candidate: updatedMetadata.contentType)

        let fpItem = await Item(
            metadata: updatedMetadata,
            parentItemIdentifier: parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: displayFileActions,
            remoteSupportsTrash: remoteInterface.supportsTrash(account: account),
            log: logger.log
        )

        return (localPath, fpItem, nil)
    }

    func fetchThumbnail(size: CGSize, domain: NSFileProviderDomain? = nil) async -> (Data?, Error?) {
        guard let thumbnailUrl = metadata.thumbnailUrl(size: size) else {
            logger.debug("Unknown thumbnail URL.", [.item: itemIdentifier, .name: filename])
            return (nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier))
        }

        logger.debug("Fetching thumbnail.", [.name: filename, .url: thumbnailUrl])

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
            logger.error("Could not acquire thumbnail.", [.item: itemIdentifier, .name: filename, .url: thumbnailUrl, .error: error])
        }

        return (data, error.fileProviderError(
            handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
        ))
    }
}

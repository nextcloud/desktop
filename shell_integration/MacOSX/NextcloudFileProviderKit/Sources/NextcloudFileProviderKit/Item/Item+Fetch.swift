//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit
import UniformTypeIdentifiers

public extension Item {
    private func downloadFileContents(
        remotePath: String,
        localPath: String,
        itemIdentifier: NSFileProviderItemIdentifier,
        domain: NSFileProviderDomain?,
        progress: Progress,
        cancellation: NetworkOperationCancellation
    ) async -> NKError {
        guard !progress.isCancelled, !Task.isCancelled else {
            return NKError(errorCode: NSURLErrorCancelled, errorDescription: CocoaError(.userCancelled).localizedDescription)
        }
        let (_, _, error) = await remoteInterface.downloadAsync(
            serverUrlFileName: remotePath,
            fileNameLocalPath: localPath,
            account: account.ncKitAccount,
            options: .init(),
            requestHandler: { cancellation.register(request: $0) },
            taskHandler: { task in
                cancellation.register(task: task)
                if let domain {
                    NSFileProviderManager(for: domain)?.register(task, forItemWithIdentifier: itemIdentifier, completionHandler: { _ in })
                }
            },
            progressHandler: { _ in }
        )
        guard !progress.isCancelled, !Task.isCancelled else {
            return NKError(errorCode: NSURLErrorCancelled, errorDescription: CocoaError(.userCancelled).localizedDescription)
        }
        return error
    }

    private func fetchDirectoryContents(
        itemIdentifier: NSFileProviderItemIdentifier,
        directoryLocalPath: String,
        directoryRemotePath: String,
        domain: NSFileProviderDomain?,
        progress: Progress,
        cancellation: NetworkOperationCancellation
    ) async throws {
        func checkFetchCancellation() throws {
            if progress.isCancelled || Task.isCancelled {
                throw CocoaError(.userCancelled)
            }
        }

        progress.totalUnitCount = 1 // Add 1 for final procedures

        // Download *everything* within this directory. What we do:
        // 1. Enumerate the contents of the directory
        // 2. Download everything within this directory
        // 3. Detect child directories
        // 4. Repeat 1 -> 3 for each child directory
        var remoteDirectoryPaths = [directoryRemotePath]
        var downloadedFileOcIds: [String] = []
        while !remoteDirectoryPaths.isEmpty {
            try checkFetchCancellation()
            let remoteDirectoryPath = remoteDirectoryPaths.removeFirst()
            let readResult = await Enumerator.readServerUrl(
                remoteDirectoryPath,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                domain: domain,
                enumeratedItemIdentifier: itemIdentifier,
                taskHandler: { cancellation.register(task: $0) },
                log: logger.log
            )
            try checkFetchCancellation()

            if let readError = readResult.error, readError != .success {
                logger.error("Could not enumerate directory contents.", [.name: metadata.fileName, .url: remoteDirectoryPath, .error: readError])

                throw readError.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                ) ?? NSFileProviderError(.cannotSynchronize)
            }

            guard var metadatas = readResult.metadatas else {
                logger.error("Could not fetch directory contents.", [.name: metadata.fileName, .url: remoteDirectoryPath])
                throw NSFileProviderError(.cannotSynchronize)
            }

            if !metadatas.isEmpty {
                metadatas.removeFirst() // Remove the dir itself
            }
            progress.totalUnitCount += Int64(metadatas.count)

            for var metadata in metadatas {
                try checkFetchCancellation()
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

                    guard let downloadingMetadata = dbManager.setStatusForItemMetadata(metadata, status: .downloading) else {
                        throw NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier)
                    }
                    metadata = downloadingMetadata
                    let error = await downloadFileContents(
                        remotePath: remotePath,
                        localPath: childLocalPath,
                        itemIdentifier: identifier,
                        domain: domain,
                        progress: progress,
                        cancellation: cancellation
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

                if !metadata.directory {
                    downloadedFileOcIds.append(metadata.ocId)
                }

                progress.completedUnitCount += 1
            }
        }

        progress.completedUnitCount += 1 // Finish off

        // Downloading a folder materializes its descendant files without going
        // through the single-file `fetchContents` refresh below, and the
        // materialized-set observer sees no DB discrepancy (the rows are already
        // downloaded==true). Nudge the descendants' ancestor containers directly so
        // "Remove downloaded items" appears on the folder, its subfolders, and up
        // to the root (#10085).
        if let domain, let manager = NSFileProviderManager(for: domain), !downloadedFileOcIds.isEmpty {
            refreshRemoveDownloadVisibility(forAncestorsOfFileOcIds: Set(downloadedFileOcIds), manager: manager, dbManager: dbManager, logger: logger)
        }
    }

    func fetchContents(
        domain: NSFileProviderDomain? = nil,
        progress: Progress = .init(),
        dbManager: FilesDatabaseManager
    ) async -> (URL?, Item?, Error?) {
        await NetworkOperationCancellation(log: logger.log).run(progress: progress) { @Sendable cancellation in
            await self.performFetchContents(domain: domain, progress: progress, dbManager: dbManager, cancellation: cancellation)
        }
    }

    internal func performFetchContents(
        domain: NSFileProviderDomain?,
        progress: Progress,
        dbManager: FilesDatabaseManager,
        cancellation: NetworkOperationCancellation
    ) async -> (URL?, Item?, Error?) {
        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, nil, CocoaError(.userCancelled))
        }
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

        defer {
            if progress.isCancelled || Task.isCancelled {
                updatedMetadata.status = Status.downloadError.rawValue
                updatedMetadata.downloaded = false
                updatedMetadata.sessionError = CocoaError(.userCancelled).localizedDescription
                dbManager.addItemMetadata(updatedMetadata)
            }
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
                    progress: progress,
                    cancellation: cancellation
                )
            } catch {
                logger.error("Could not fetch directory contents.", [.item: ocId, .error: error])

                updatedMetadata.status = Status.downloadError.rawValue
                updatedMetadata.sessionError = error.localizedDescription
                dbManager.addItemMetadata(updatedMetadata)
                return (nil, nil, error)
            }

        } else {
            let error = await downloadFileContents(
                remotePath: serverUrlFileName,
                localPath: localPath.path,
                itemIdentifier: itemIdentifier,
                domain: domain,
                progress: progress,
                cancellation: cancellation
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

        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, nil, CocoaError(.userCancelled))
        }
        logger.debug("Acquired contents of item.", [.item: ocId, .name: updatedMetadata.fileName])

        if !isDirectory, updatedMetadata.contentType != UTType.aliasFile.identifier {
            if let fileHandle = try? FileHandle(forReadingFrom: localPath) {
                let magic = fileHandle.readData(ofLength: 4)
                try? fileHandle.close()
                // Apple Bookmark/Alias format magic bytes: "book" (0x62 0x6F 0x6F 0x6B)
                if magic == Data([0x62, 0x6F, 0x6F, 0x6B]) {
                    logger.debug("Detected macOS alias file by magic number.", [.name: updatedMetadata.fileName])
                    updatedMetadata.contentType = UTType.aliasFile.identifier
                }
            }
        }

        updatedMetadata.status = Status.normal.rawValue
        updatedMetadata.downloaded = true
        // HACK: We were previously failing to correctly set the uploaded state to true for
        // enumerated items. Fix it now to ensure we do not show "waiting for upload" when
        // having downloaded incorrectly enumerated files
        updatedMetadata.uploaded = true
        updatedMetadata.sessionError = ""

        dbManager.addItemMetadata(updatedMetadata)

        // A newly downloaded file changes the "Remove download" visibility of every
        // ancestor folder and the root. This must happen here, not via the
        // materialized-set observer: `downloaded` is already persisted above,
        // before the system re-enumerates its materialized set, so the observer's
        // reconciliation would see no change (#10085).
        if !isDirectory, let domain, let manager = NSFileProviderManager(for: domain) {
            refreshRemoveDownloadVisibility(forAncestorsOfFileOcIds: [ocId], manager: manager, dbManager: dbManager, logger: logger)
        }

        let parentItemIdentifier = await dbManager.parentItemIdentifierWithRemoteFallback(
            fromMetadata: metadata,
            remoteInterface: remoteInterface,
            account: account
        )
        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, nil, CocoaError(.userCancelled))
        }
        guard let parentItemIdentifier else {
            logger.error("Could not find parent item id for file.", [.name: metadata.fileName])

            return (nil, nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier))
        }

        let displayFileActions = await Item.typeHasApplicableContextMenuItems(account: account, remoteInterface: remoteInterface, candidate: updatedMetadata.contentType)
        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, nil, CocoaError(.userCancelled))
        }

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

        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, nil, CocoaError(.userCancelled))
        }
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

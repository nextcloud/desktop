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
        operation: DownloadOperation
    ) async throws {
        func checkFetchCancellation() throws {
            if progress.isCancelled || Task.isCancelled {
                throw operation.error(for: CocoaError(.userCancelled))
            }
        }

        func checkDirectoryIsPresent(_ directoryOperation: DownloadOperation) throws {
            guard let metadata = dbManager.itemMetadata(ocId: directoryOperation.ocId), !metadata.deleted else {
                throw NSFileProviderError(.cannotSynchronize)
            }
        }

        progress.totalUnitCount = 1 // Add 1 for final procedures

        var remoteDirectories = [(path: directoryRemotePath, operation: operation)]
        var pendingDirectoryOperations: [DownloadOperation] = []
        defer {
            // Release unfinished child directories; the root fetch reports the operation's error.
            for directoryOperation in pendingDirectoryOperations {
                try? dbManager.finishDownload(directoryOperation, status: .normal, downloaded: false)
            }
        }
        var downloadedFileOcIds: [String] = []
        while !remoteDirectories.isEmpty {
            try checkFetchCancellation()
            let directory = remoteDirectories.removeFirst()
            try checkDirectoryIsPresent(directory.operation)
            let remoteDirectoryPath = directory.path
            let readResult = await Enumerator.readServerUrl(
                remoteDirectoryPath,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                domain: domain,
                enumeratedItemIdentifier: itemIdentifier,
                taskHandler: { operation.cancellation.register(task: $0) },
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

            try checkDirectoryIsPresent(directory.operation)
            if !metadatas.isEmpty {
                metadatas.removeFirst() // Remove the dir itself
            }
            // Server listings can include children intentionally removed from the local view.
            metadatas = try dbManager.directoryDownloadMetadatas(metadatas)
            progress.totalUnitCount += Int64(metadatas.count)

            for metadata in metadatas {
                try checkFetchCancellation()
                let remotePath = metadata.remotePath()
                let relativePath =
                    remotePath.replacingOccurrences(of: directoryRemotePath, with: "")
                let childLocalPath = directoryLocalPath + relativePath
                let childOperation = operation.childOperation(for: metadata.ocId)

                guard dbManager.beginDownload(childOperation) != nil else {
                    throw NSFileProviderError(.cannotSynchronize)
                }
                if metadata.directory {
                    pendingDirectoryOperations.append(childOperation)
                    remoteDirectories.append((path: remotePath, operation: childOperation))
                    try FileManager.default.createDirectory(
                        at: URL(fileURLWithPath: childLocalPath),
                        withIntermediateDirectories: true,
                        attributes: nil
                    )
                } else {
                    let identifier = NSFileProviderItemIdentifier(metadata.ocId)

                    let error = await downloadFileContents(
                        remotePath: remotePath,
                        localPath: childLocalPath,
                        itemIdentifier: identifier,
                        domain: domain,
                        progress: progress,
                        cancellation: operation.cancellation
                    )

                    guard error == .success else {
                        logger.error("Could not acquire contents of item.", [.name: metadata.fileName, .url: remotePath, .error: error])
                        let cancelled = error.errorCode == NSURLErrorCancelled
                        try? dbManager.finishDownload(
                            childOperation,
                            status: cancelled ? .normal : .downloadError,
                            downloaded: false,
                            error: cancelled ? nil : error.errorDescription
                        )
                        throw operation.error(for: error.fileProviderError(
                            handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                        ) ?? NSFileProviderError(.cannotSynchronize))
                    }
                    try dbManager.finishDownload(childOperation, status: .normal, downloaded: true)
                    downloadedFileOcIds.append(metadata.ocId)
                }

                progress.completedUnitCount += 1
            }
        }

        while let directoryOperation = pendingDirectoryOperations.last {
            try checkFetchCancellation()
            try dbManager.finishDownload(directoryOperation, status: .normal, downloaded: true)
            pendingDirectoryOperations.removeLast()
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

        let isDirectory = contentType.conforms(to: .directory)
        // Give each fetch its own destination so cleanup cannot remove another fetch's contents.
        let localPath = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let operation = DownloadOperation(ocId: ocId, cancellation: cancellation, log: logger.log)
        guard var updatedMetadata = dbManager.beginDownload(operation) else {
            logger.error("Could not acquire updated metadata, unable to update item status to downloading.", [.item: itemIdentifier])

            return (
                nil,
                nil,
                NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier)
            )
        }

        var contentsReturned = false
        var fetchErrorDescription: String?
        var fetchWasCancelled = false
        defer {
            if !contentsReturned {
                let cancelled = fetchWasCancelled || progress.isCancelled || Task.isCancelled
                try? dbManager.finishDownload(
                    operation,
                    status: cancelled ? .normal : .downloadError,
                    downloaded: false,
                    error: cancelled ? nil : fetchErrorDescription
                )
                do {
                    try FileManager.default.removeItem(at: localPath)
                } catch CocoaError.fileNoSuchFile {
                    // Failure may precede creation of the temporary contents.
                } catch {
                    logger.error("Could not remove unreturned download contents.", [.item: ocId, .url: localPath, .error: error])
                }
            }
        }

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

                fetchErrorDescription = error.localizedDescription
                return (nil, nil, operation.error(for: error))
            }

            do {
                try await fetchDirectoryContents(
                    itemIdentifier: itemIdentifier,
                    directoryLocalPath: localPath.path,
                    directoryRemotePath: serverUrlFileName,
                    domain: domain,
                    progress: progress,
                    operation: operation
                )
            } catch {
                logger.error("Could not fetch directory contents.", [.item: ocId, .error: error])

                fetchWasCancelled = error is CancellationError || (error as? CocoaError)?.code == .userCancelled
                fetchErrorDescription = error.localizedDescription
                return (nil, nil, operation.error(for: error))
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

                fetchWasCancelled = error.errorCode == NSURLErrorCancelled
                fetchErrorDescription = error.errorDescription
                return (nil, nil, operation.error(for: error.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                ) ?? NSFileProviderError(.cannotSynchronize)))
            }
        }

        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, nil, operation.error(for: CocoaError(.userCancelled)))
        }
        logger.debug("Acquired contents of item.", [.item: ocId, .name: updatedMetadata.fileName])

        var detectedContentType: String?
        if !isDirectory, updatedMetadata.contentType != UTType.aliasFile.identifier, let fileHandle = try? FileHandle(forReadingFrom: localPath) {
            let magic = fileHandle.readData(ofLength: 4)
            try? fileHandle.close()
            // Apple Bookmark/Alias format magic bytes: "book" (0x62 0x6F 0x6F 0x6B)
            if magic == Data([0x62, 0x6F, 0x6F, 0x6B]) {
                logger.debug("Detected macOS alias file by magic number.", [.name: updatedMetadata.fileName])
                updatedMetadata.contentType = UTType.aliasFile.identifier
                detectedContentType = UTType.aliasFile.identifier
            }
        }

        updatedMetadata.status = Status.normal.rawValue
        updatedMetadata.downloaded = true
        // HACK: We were previously failing to correctly set the uploaded state to true for
        // enumerated items. Fix it now to ensure we do not show "waiting for upload" when
        // having downloaded incorrectly enumerated files
        updatedMetadata.uploaded = true
        updatedMetadata.sessionError = ""

        let parentItemIdentifier = await dbManager.parentItemIdentifierWithRemoteFallback(
            fromMetadata: metadata,
            remoteInterface: remoteInterface,
            account: account,
            taskHandler: { cancellation.register(task: $0) }
        )
        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, nil, operation.error(for: CocoaError(.userCancelled)))
        }
        guard let parentItemIdentifier else {
            logger.error("Could not find parent item id for file.", [.name: metadata.fileName])

            let error = NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier)
            fetchErrorDescription = error.localizedDescription
            return (nil, nil, operation.error(for: error))
        }

        let displayFileActions = await Item.typeHasApplicableContextMenuItems(account: account, remoteInterface: remoteInterface, candidate: updatedMetadata.contentType)
        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, nil, operation.error(for: CocoaError(.userCancelled)))
        }

        let remoteSupportsTrash = await remoteInterface.supportsTrash(account: account)
        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, nil, operation.error(for: CocoaError(.userCancelled)))
        }
        do {
            try dbManager.finishDownload(operation, status: .normal, downloaded: true, contentType: detectedContentType)
        } catch {
            return (nil, nil, operation.error(for: error))
        }

        // Refresh ancestor actions after the successful download state has been saved (#10085).
        if !isDirectory, let domain, let manager = NSFileProviderManager(for: domain) {
            refreshRemoveDownloadVisibility(forAncestorsOfFileOcIds: [ocId], manager: manager, dbManager: dbManager, logger: logger)
        }
        let fpItem = Item(
            metadata: updatedMetadata,
            parentItemIdentifier: parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: displayFileActions,
            remoteSupportsTrash: remoteSupportsTrash,
            log: logger.log
        )

        // The caller owns the contents once we return them, even if cancellation arrives now.
        contentsReturned = true
        return (localPath, fpItem, nil)
    }

    internal func performFetchThumbnail(
        size: CGSize,
        domain: NSFileProviderDomain?,
        progress: Progress,
        cancellation: NetworkOperationCancellation
    ) async -> (Data?, Error?) {
        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, CocoaError(.userCancelled))
        }
        guard let thumbnailUrl = metadata.thumbnailUrl(size: size) else {
            logger.debug("Unknown thumbnail URL.", [.item: itemIdentifier, .name: filename])
            return (nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier))
        }

        logger.debug("Fetching thumbnail.", [.name: filename, .url: thumbnailUrl])

        let (_, data, error) = await remoteInterface.downloadThumbnail(
            url: thumbnailUrl, account: account, options: .init(), taskHandler: { task in
                cancellation.register(task: task)
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard !progress.isCancelled, !Task.isCancelled else {
            return (nil, CocoaError(.userCancelled))
        }
        if error != .success {
            logger.error("Could not acquire thumbnail.", [.item: itemIdentifier, .name: filename, .url: thumbnailUrl, .error: error])
        }

        return (data, error.fileProviderError(
            handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
        ))
    }
}

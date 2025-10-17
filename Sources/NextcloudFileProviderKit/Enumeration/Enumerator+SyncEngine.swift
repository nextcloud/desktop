//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

extension Enumerator {
    static func handlePagedReadResults(
        files: [NKFile], pageIndex: Int, dbManager: FilesDatabaseManager
    ) -> (metadatas: [SendableItemMetadata]?, error: NKError?) {
        // First PROPFIND contains the target item, but we do not want to report this in the
        // retrieved metadatas (the enumeration observers don't expect you to enumerate the
        // target item, hence why we always strip the target item out)
        let startIndex = pageIndex > 0 ? 0 : 1
        if pageIndex == 0 {
            guard let firstFile = files.first else { return (nil, .invalidResponseError) }
            var metadata = firstFile.toItemMetadata()
            if metadata.directory {
                metadata.visitedDirectory = true
                if let existingMetadata = dbManager.itemMetadata(ocId: metadata.ocId) {
                    metadata.downloaded = existingMetadata.downloaded
                    metadata.keepDownloaded = existingMetadata.keepDownloaded
                }
            }
            dbManager.addItemMetadata(metadata)
        }
        let metadatas = files[startIndex...].map { $0.toItemMetadata() }
        metadatas.forEach { dbManager.addItemMetadata($0) }
        return (metadatas, nil)
    }

    // With paginated requests, you do not have a way to know what has changed remotely when
    // handling the result of an individual PROPFIND request. When handling a paginated read this
    // therefore only returns the acquired metadatas.
    static func handleDepth1ReadFileOrFolder(
        serverUrl: String,
        account: Account,
        dbManager: FilesDatabaseManager,
        files: [NKFile],
        pageIndex: Int?,
        log: any FileProviderLogging
    ) async -> (
        metadatas: [SendableItemMetadata]?,
        newMetadatas: [SendableItemMetadata]?,
        updatedMetadatas: [SendableItemMetadata]?,
        deletedMetadatas: [SendableItemMetadata]?,
        readError: NKError?
    ) {
        let logger = FileProviderLogger(category: "Enumerator", log: log)

        logger.debug("Starting async conversion of NKFiles for serverUrl: \(serverUrl) for user: \(account.ncKitAccount)")

        if let pageIndex {
            let (metadatas, error) =
                handlePagedReadResults(files: files, pageIndex: pageIndex, dbManager: dbManager)
            return (metadatas, nil, nil, nil, error)
        }

        guard var (directory, _, files) = await files.toSendableDirectoryMetadata(account: account, directoryToRead: serverUrl) else {
            logger.error("Failed to convert array of NKFile to directory and files metadata objects!")
            return (nil, nil, nil, nil, .invalidData)
        }

        // STORE DATA FOR CURRENTLY SCANNED DIRECTORY
        guard directory.directory else {
            logger.error("Expected directory metadata but received file metadata!", [.url: serverUrl])
            return (nil, nil, nil, nil, .invalidData)
        }

        if let existingMetadata = dbManager.itemMetadata(ocId: directory.ocId) {
            directory.downloaded = existingMetadata.downloaded
            directory.keepDownloaded = existingMetadata.keepDownloaded
        }

        directory.visitedDirectory = true

        files.insert(directory, at: 0)

        let changedMetadatas = dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: serverUrl,
            updatedMetadatas: files,
            keepExistingDownloadState: true
        )

        return (
            files,
            changedMetadatas.newMetadatas,
            changedMetadatas.updatedMetadatas,
            changedMetadatas.deletedMetadatas,
            nil
        )
    }

    // READ THIS CAREFULLY.
    //
    // This method supports paginated and non-paginated reads. Handled by the pageSettings argument.
    // Paginated reads is used by enumerateItems, non-paginated reads is used by enumerateChanges.
    //
    // Paginated reads WILL NOT HANDLE REMOVAL OF REMOTELY DELETED ITEMS FROM THE LOCAL DATABASE.
    // Paginated reads WILL ONLY REPORT THE FILES DISCOVERED REMOTELY.
    // This means that if you decide to use this method to implement change enumeration, you will
    // have to collect the full results of all the pages before proceeding with discovering what
    // has changed relative to the state of the local database -- manually!
    //
    // Non-paginated reads will update the database with all of the discovered files and folders
    // that have been found to be created, updated, and deleted. No extra work required.
    static func readServerUrl(
        _ serverUrl: String,
        pageSettings: (page: NSFileProviderPage?, index: Int, size: Int)? = nil,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        domain: NSFileProviderDomain? = nil,
        enumeratedItemIdentifier: NSFileProviderItemIdentifier? = nil,
        depth: EnumerateDepth = .targetAndDirectChildren,
        log: any FileProviderLogging
    ) async -> (
        metadatas: [SendableItemMetadata]?,
        newMetadatas: [SendableItemMetadata]?,
        updatedMetadatas: [SendableItemMetadata]?,
        deletedMetadatas: [SendableItemMetadata]?,
        nextPage: EnumeratorPageResponse?,
        readError: NKError?
    ) {
        let logger = FileProviderLogger(category: "Enumerator", log: log)

        logger.debug("Starting to read server URL.", [.url: serverUrl])

        let options: NKRequestOptions = if let pageSettings {
            .init(
                page: pageSettings.page,
                offset: pageSettings.index * pageSettings.size,
                count: pageSettings.size
            )
        } else {
            .init()
        }

        let (_, files, data, error) = await remoteInterface.enumerate(
            remotePath: serverUrl,
            depth: depth,
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            account: account,
            options: options,
            taskHandler: { task in
                if let domain, let enumeratedItemIdentifier {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: enumeratedItemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard error == .success else {
            logger.error("Read of URL did fail.", [.error: error, .url: serverUrl])
            return (nil, nil, nil, nil, nil, error)
        }

        guard let data else {
            logger.error("\(depth.rawValue) depth read of url \(serverUrl) did not return data.")
            return (nil, nil, nil, nil, nil, error)
        }

        // This will be nil if the page settings were also nil, as the server will not give us the
        // pagination-related headers.
        let nextPage = EnumeratorPageResponse(nkResponseData: data, index: (pageSettings?.index ?? 0) + 1, log: log)

        guard let receivedFile = files.first else {
            logger.error("Received no items.", [.url: serverUrl])
            // This is technically possible when doing a paginated request with the index too high.
            // It's technically not an error reply.
            return ([], nil, nil, nil, nextPage, nil)
        }

        // Generally speaking a PROPFIND will provide the target of the PROPFIND as the first result
        // That is NOT the case for paginated results with offsets
        let isFollowUpPaginatedRequest = (pageSettings?.page != nil && pageSettings?.index ?? 0 > 0)
        if !isFollowUpPaginatedRequest {
            guard receivedFile.directory ||
                serverUrl == dbManager.account.davFilesUrl ||
                receivedFile.fullUrlMatches(dbManager.account.davFilesUrl + "/.") ||
                (receivedFile.fileName == NextcloudKit.shared.nkCommonInstance.rootFileName && receivedFile.serverUrl == dbManager.account.davFilesUrl)
            else {
                logger.debug("Read item is a file, converting.", [.url: serverUrl])
                var metadata = receivedFile.toItemMetadata()
                let existing = dbManager.itemMetadata(ocId: metadata.ocId)
                let isNew = existing == nil
                let newItems: [SendableItemMetadata] = isNew ? [metadata] : []
                metadata.lockToken = existing?.lockToken
                let updatedItems: [SendableItemMetadata] = isNew ? [] : [metadata]
                metadata.downloaded = existing?.downloaded == true
                metadata.keepDownloaded = existing?.keepDownloaded == true
                dbManager.addItemMetadata(metadata)
                return ([metadata], newItems, updatedItems, nil, nextPage, nil)
            }
        }

        if depth == .target {
            var metadata = receivedFile.toItemMetadata()
            let existing = dbManager.itemMetadata(ocId: metadata.ocId)
            let isNew = existing == nil
            let updatedMetadatas = isNew ? [] : [metadata]
            let newMetadatas = isNew ? [metadata] : []

            metadata.downloaded = existing?.downloaded == true
            metadata.keepDownloaded = existing?.keepDownloaded == true
            dbManager.addItemMetadata(metadata)

            return ([metadata], newMetadatas, updatedMetadatas, nil, nextPage, nil)
        } else if depth == .targetAndDirectChildren {
            let (allMetadatas, newMetadatas, updatedMetadatas, deletedMetadatas, readError) = await handleDepth1ReadFileOrFolder(
                serverUrl: serverUrl,
                account: account,
                dbManager: dbManager,
                files: files,
                pageIndex: pageSettings?.index,
                log: logger.log
            )

            return (allMetadatas, newMetadatas, updatedMetadatas, deletedMetadatas, nextPage, readError)
        } else if let pageIndex = pageSettings?.index {
            let (metadatas, error) = handlePagedReadResults(
                files: files, pageIndex: pageIndex, dbManager: dbManager
            )
            return (metadatas, nil, nil, nil, nextPage, error)
        } else {
            // Infinite depth unpaged reads are a bad idea
            return (nil, nil, nil, nil, nil, .invalidResponseError)
        }
    }
}

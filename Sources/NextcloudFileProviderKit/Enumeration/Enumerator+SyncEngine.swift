/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import FileProvider
import NextcloudKit
import OSLog

extension Enumerator {
    func fullRecursiveScan(
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        scanChangesOnly: Bool
    ) async -> (
        metadatas: [SendableItemMetadata],
        newMetadatas: [SendableItemMetadata],
        updatedMetadatas: [SendableItemMetadata],
        deletedMetadatas: [SendableItemMetadata],
        error: NKError?
    ) {
        let results = await self.scanRecursively(
            Item.rootContainer(
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                remoteSupportsTrash: await remoteInterface.supportsTrash(account: account)
            ).metadata,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            scanChangesOnly: scanChangesOnly
        )

        // Run a check to ensure files deleted in one location are not updated in another
        // (e.g. when moved)
        // The recursive scan provides us with updated/deleted metadatas only on a folder by
        // folder basis; so we need to check we are not simultaneously marking a moved file as
        // deleted and updated
        var checkedDeletedMetadatas = results.deletedMetadatas

        for updatedMetadata in results.updatedMetadatas {
            guard let matchingDeletedMetadataIdx = checkedDeletedMetadatas.firstIndex(
                where: { $0.ocId == updatedMetadata.ocId }
            ) else { continue }

            checkedDeletedMetadatas.remove(at: matchingDeletedMetadataIdx)
        }

        return (
            results.metadatas,
            results.newMetadatas,
            results.updatedMetadatas,
            checkedDeletedMetadatas, 
            results.error
        )
    }

    private func scanRecursively(
        _ directoryMetadata: SendableItemMetadata,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        scanChangesOnly: Bool
    ) async -> (
        metadatas: [SendableItemMetadata],
        newMetadatas: [SendableItemMetadata],
        updatedMetadatas: [SendableItemMetadata],
        deletedMetadatas: [SendableItemMetadata],
        error: NKError?
    ) {
        if isInvalidated {
            return ([], [], [], [], nil)
        }

        assert(directoryMetadata.directory, "Can only recursively scan a directory.")

        // Will include results of recursive calls
        var allMetadatas: [SendableItemMetadata] = []
        var allNewMetadatas: [SendableItemMetadata] = []
        var allUpdatedMetadatas: [SendableItemMetadata] = []
        var allDeletedMetadatas: [SendableItemMetadata] = []

        let itemServerUrl =
            directoryMetadata.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue
                ? account.davFilesUrl
                : directoryMetadata.serverUrl + "/" + directoryMetadata.fileName

        Self.logger.debug("About to read: \(itemServerUrl, privacy: .public)")

        let (
            metadatas, newMetadatas, updatedMetadatas, deletedMetadatas, _, readError
        ) = await Self.readServerUrl(
            itemServerUrl,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            domain: domain,
            enumeratedItemIdentifier: enumeratedItemIdentifier
        )

        if let readError, readError != .success {
            // Is the error is that we have found matching etags on this item, then ignore it
            // if we are doing a full rescan
            if readError.isNoChangesError, scanChangesOnly {
                Self.logger.info("No changes in \(self.serverUrl) and only scanning changes.")
            } else {
                Self.logger.error(
                    """
                    Finishing enumeration of changes at \(itemServerUrl, privacy: .public)
                        with \(readError.errorDescription, privacy: .public)
                    """
                )

                if readError.isNotFoundError {
                    Self.logger.info(
                        """
                        404 error means item no longer exists.
                            Deleting metadata and reporting as deletion without error.
                        """
                    )

                    if let deletedMetadatas = dbManager.deleteDirectoryAndSubdirectoriesMetadata(
                        ocId: directoryMetadata.ocId
                    ) {
                        allDeletedMetadatas += deletedMetadatas
                    } else {
                        Self.logger.error(
                            """
                            An error occurred while trying to delete directory in database,
                            children not found in recursive scan
                            """
                        )
                    }

                } else if readError.isNoChangesError {  // All is well, just no changed etags
                    Self.logger.info(
                        "Error was to say no changed files, not bad error. Won't check children."
                    )

                } else if readError.isUnauthenticatedError || readError.isCouldntConnectError {
                    Self.logger.error(
                        "Error will affect next enumerated items, so stopping enumeration."
                    )
                    return ([], [] , [], [], readError)
                }
            }
        }

        Self.logger.info(
            """
            Finished reading serverUrl: \(itemServerUrl, privacy: .public)
            for user: \(account.ncKitAccount, privacy: .public)
            """
        )

        if let metadatas {
            allMetadatas += metadatas
        } else {
            Self.logger.warning(
                """
                Nil metadatas received in change read at \(itemServerUrl, privacy: .public)
                for user: \(account.ncKitAccount, privacy: .public)
                """
            )
        }

        if let newMetadatas {
            allNewMetadatas += newMetadatas
        } else {
            Self.logger.warning(
                """
                Nil new metadatas received in change read at \(itemServerUrl, privacy: .public)
                for user: \(account.ncKitAccount, privacy: .public)
                """
            )
        }

        if let updatedMetadatas {
            allUpdatedMetadatas += updatedMetadatas
        } else {
            Self.logger.warning(
                """
                Nil updated metadatas received in change read at \(itemServerUrl, privacy: .public)
                for user: \(account.ncKitAccount, privacy: .public)
                """
            )
        }

        if let deletedMetadatas {
            allDeletedMetadatas += deletedMetadatas
        } else {
            Self.logger.warning(
                """
                Nil deleted metadatas received in change read at \(itemServerUrl, privacy: .public)
                for user: \(account.ncKitAccount, privacy: .public)
                """
            )
        }

        var childDirectoriesToScan: [SendableItemMetadata] = []
        var candidateMetadatas: [SendableItemMetadata]

        if scanChangesOnly {
            candidateMetadatas = allUpdatedMetadatas + allNewMetadatas
        } else {
            candidateMetadatas = allMetadatas
        }

        for candidateMetadata in candidateMetadatas where candidateMetadata.directory {
            childDirectoriesToScan.append(candidateMetadata)
        }

        Self.logger.debug(
            "Candidate metadatas for further scan: \(childDirectoriesToScan, privacy: .public)"
        )

        if childDirectoriesToScan.isEmpty {
            return (
                metadatas: allMetadatas, 
                newMetadatas: allNewMetadatas,
                updatedMetadatas: allUpdatedMetadatas, 
                deletedMetadatas: allDeletedMetadatas,
                nil
            )
        }

        for childDirectory in childDirectoriesToScan {
            let childDirectoryUrl = childDirectory.serverUrl + "/" + childDirectory.fileName
            Self.logger.debug(
                """
                About to recursively scan: \(childDirectoryUrl, privacy: .public)
                    with etag: \(childDirectory.etag, privacy: .public)
                """
            )
            let childScanResult = await scanRecursively(
                childDirectory,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                scanChangesOnly: scanChangesOnly
            )

            allMetadatas += childScanResult.metadatas
            allNewMetadatas += childScanResult.newMetadatas
            allUpdatedMetadatas += childScanResult.updatedMetadatas
            allDeletedMetadatas += childScanResult.deletedMetadatas
        }

        return (
            metadatas: allMetadatas, newMetadatas: allNewMetadatas,
            updatedMetadatas: allUpdatedMetadatas,
            deletedMetadatas: allDeletedMetadatas, nil
        )
    }

    static func handlePagedReadResults(
        files: [NKFile], pageIndex: Int, dbManager: FilesDatabaseManager
    ) -> (metadatas: [SendableItemMetadata]?, error: NKError?) {
        // First PROPFIND contains the target item, but we do not want to report this in the
        // retrieved metadatas (the enumeration observers don't expect you to enumerate the
        // target item, hence why we always strip the target item out)
        let startIndex = pageIndex > 0 ? 0 : 1
        if pageIndex == 0 {
            guard let firstFile = files.first else { return (nil, .invalidResponseError) }
            // Do not ingest metadata for the root container
            if !firstFile.fullUrlMatches(dbManager.account.davFilesUrl),
               !firstFile.fullUrlMatches(dbManager.account.davFilesUrl + "/.")
            {
                var metadata = firstFile.toItemMetadata()
                if metadata.directory,
                   let existingMetadata = dbManager.itemMetadata(ocId: metadata.ocId)
                {
                    metadata.downloaded = existingMetadata.downloaded
                }
                dbManager.addItemMetadata(metadata)
            }
        }
        let metadatas = files[startIndex..<files.count].map { $0.toItemMetadata() }
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
        pageIndex: Int?
    ) async -> (
        metadatas: [SendableItemMetadata]?,
        newMetadatas: [SendableItemMetadata]?,
        updatedMetadatas: [SendableItemMetadata]?,
        deletedMetadatas: [SendableItemMetadata]?,
        readError: NKError?
    ) {
        Self.logger.debug(
            """
            Starting async conversion of NKFiles for serverUrl: \(serverUrl, privacy: .public)
                for user: \(account.ncKitAccount, privacy: .public)
            """
        )

        if let pageIndex {
            let (metadatas, error) =
                handlePagedReadResults(files: files, pageIndex: pageIndex, dbManager: dbManager)
            return (metadatas, nil, nil, nil, error)
        }

        guard var (directoryMetadata, _, metadatas) =
            await files.toDirectoryReadMetadatas(account: account)
        else {
            Self.logger.error("Could not convert NKFiles to DirectoryReadMetadatas!")
            return (nil, nil, nil, nil, .invalidData)
        }

        // STORE DATA FOR CURRENTLY SCANNED DIRECTORY
        if serverUrl != account.davFilesUrl {
            if let existingMetadata = dbManager.itemMetadata(ocId: directoryMetadata.ocId) {
                directoryMetadata.downloaded = existingMetadata.downloaded
            }
            dbManager.addItemMetadata(directoryMetadata)
        }

        let changedMetadatas = dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: serverUrl,
            updatedMetadatas: metadatas,
            keepExistingDownloadState: true
        )

        return (
            metadatas,
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
    // Paginated reads WILL ONLY REPORT THE FILES DISCOVERED LOCALLY.
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
        depth: EnumerateDepth = .targetAndDirectChildren
    ) async -> (
        metadatas: [SendableItemMetadata]?,
        newMetadatas: [SendableItemMetadata]?,
        updatedMetadatas: [SendableItemMetadata]?,
        deletedMetadatas: [SendableItemMetadata]?,
        nextPage: EnumeratorPageResponse?,
        readError: NKError?
    ) {
        let ncKitAccount = account.ncKitAccount

        Self.logger.debug(
            """
            Starting to read serverUrl: \(serverUrl, privacy: .public)
                for user: \(ncKitAccount, privacy: .public)
                at depth \(depth.rawValue, privacy: .public).
                username: \(account.username, privacy: .public),
                password is empty: \(account.password == "" ? "EMPTY" : "NOT EMPTY"),
                serverUrl: \(account.serverUrl, privacy: .public)
            """
        )

        let options: NKRequestOptions
        if let pageSettings {
            options = .init(
                page: pageSettings.page,
                offset: pageSettings.index * pageSettings.size,
                count: pageSettings.size
            )
        } else {
            options = .init()
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
            Self.logger.error(
                """
                \(depth.rawValue, privacy: .public) depth read of url \(serverUrl, privacy: .public)
                did not complete successfully, error: \(error.errorDescription, privacy: .public)
                """
            )
            return (nil, nil, nil, nil, nil, error)
        }

        guard let data else {
            Self.logger.error(
                """
                \(depth.rawValue, privacy: .public) depth read of url \(serverUrl, privacy: .public)
                    did not return data.
                """
            )
            return (nil, nil, nil, nil, nil, error)
        }

        // This will be nil if the page settings were also nil, as the server will not give us the
        // pagination-related headers.
        let nextPage = EnumeratorPageResponse(
            nkResponseData: data, index: (pageSettings?.index ?? 0) + 1
        )

        guard let receivedFile = files.first else {
            Self.logger.error(
                """
                Received no items from readFileOrFolder of \(serverUrl, privacy: .public),
                not much we can do...
                """
            )
            return (nil, nil, nil, nil, nextPage, error)
        }

        // Generally speaking a PROPFIND will provide the target of the PROPFIND as the first result
        // That is NOT the case for paginated results with offsets
        let isFollowUpPaginatedRequest = (pageSettings?.page != nil && pageSettings?.index ?? 0 > 0)
        if !isFollowUpPaginatedRequest {
            guard receivedFile.directory ||
                  serverUrl == dbManager.account.davFilesUrl ||
                  receivedFile.fullUrlMatches(dbManager.account.davFilesUrl + "/.")
            else {
                Self.logger.debug(
                    """
                    Read item is a file.
                        Converting NKfile for serverUrl: \(serverUrl, privacy: .public)
                        for user: \(account.ncKitAccount, privacy: .public)
                    """
                )
                var metadata = receivedFile.toItemMetadata()
                let existing = dbManager.itemMetadata(ocId: metadata.ocId)
                let isNew = existing == nil
                let newItems: [SendableItemMetadata] = isNew ? [metadata] : []
                let updatedItems: [SendableItemMetadata] = isNew ? [] : [metadata]
                metadata.downloaded = existing?.downloaded == true
                dbManager.addItemMetadata(metadata)
                return ([metadata], newItems, updatedItems, nil, nextPage, nil)
            }
        }

        if depth == .target {
            if serverUrl == account.davFilesUrl {
                return (nil, nil, nil, nil, nextPage, nil)
            } else {
                var metadata = receivedFile.toItemMetadata()
                let existing = dbManager.itemMetadata(ocId: metadata.ocId)
                let isNew = existing == nil
                let updatedMetadatas = isNew ? [] : [metadata]
                let newMetadatas = isNew ? [metadata] : []

                metadata.downloaded = existing?.downloaded == true
                dbManager.addItemMetadata(metadata)

                return ([metadata], newMetadatas, updatedMetadatas, nil, nextPage, nil)
            }
        } else if depth == .targetAndDirectChildren {
            let (
                allMetadatas, newMetadatas, updatedMetadatas, deletedMetadatas, readError
            ) = await handleDepth1ReadFileOrFolder(
                serverUrl: serverUrl, 
                account: account,
                dbManager: dbManager,
                files: files,
                pageIndex: pageSettings?.index
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

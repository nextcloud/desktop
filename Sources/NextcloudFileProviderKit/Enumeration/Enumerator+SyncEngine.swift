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
        ncAccount: Account,
        ncKit: NextcloudKit,
        scanChangesOnly: Bool,
        completionHandler: @escaping (
            _ metadatas: [ItemMetadata],
            _ newMetadatas: [ItemMetadata],
            _ updatedMetadatas: [ItemMetadata],
            _ deletedMetadatas: [ItemMetadata],
            _ error: NKError?
        ) -> Void
    ) {
        let rootContainerDirectoryMetadata = ItemMetadata()
        rootContainerDirectoryMetadata.directory = true
        rootContainerDirectoryMetadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue

        // Create a serial dispatch queue
        let dispatchQueue = DispatchQueue(
            label: "recursiveChangeEnumerationQueue", qos: .userInitiated)

        dispatchQueue.async {
            let results = self.scanRecursively(
                rootContainerDirectoryMetadata,
                ncAccount: ncAccount,
                ncKit: ncKit,
                scanChangesOnly: scanChangesOnly)

            // Run a check to ensure files deleted in one location are not updated in another 
            // (e.g. when moved)
            // The recursive scan provides us with updated/deleted metadatas only on a folder by
            // folder basis; so we need to check we are not simultaneously marking a moved file as
            // deleted and updated
            var checkedDeletedMetadatas = results.deletedMetadatas

            for updatedMetadata in results.updatedMetadatas {
                guard
                    let matchingDeletedMetadataIdx = checkedDeletedMetadatas.firstIndex(where: {
                        $0.ocId == updatedMetadata.ocId
                    })
                else {
                    continue
                }

                checkedDeletedMetadatas.remove(at: matchingDeletedMetadataIdx)
            }

            DispatchQueue.main.async {
                completionHandler(
                    results.metadatas, results.newMetadatas, results.updatedMetadatas,
                    checkedDeletedMetadatas, results.error)
            }
        }
    }

    private func scanRecursively(
        _ directoryMetadata: ItemMetadata,
        ncAccount: Account,
        ncKit: NextcloudKit,
        scanChangesOnly: Bool
    ) -> (
        metadatas: [ItemMetadata],
        newMetadatas: [ItemMetadata],
        updatedMetadatas: [ItemMetadata],
        deletedMetadatas: [ItemMetadata],
        error: NKError?
    ) {
        if isInvalidated {
            return ([], [], [], [], nil)
        }

        assert(directoryMetadata.directory, "Can only recursively scan a directory.")

        // Will include results of recursive calls
        var allMetadatas: [ItemMetadata] = []
        var allNewMetadatas: [ItemMetadata] = []
        var allUpdatedMetadatas: [ItemMetadata] = []
        var allDeletedMetadatas: [ItemMetadata] = []

        let dbManager = FilesDatabaseManager.shared
        let dispatchGroup = DispatchGroup()  // TODO: Maybe own thread?

        dispatchGroup.enter()

        var criticalError: NKError?
        let itemServerUrl =
            directoryMetadata.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue
            ? ncAccount.davFilesUrl : directoryMetadata.serverUrl + "/" + directoryMetadata.fileName

        Self.logger.debug("About to read: \(itemServerUrl, privacy: .public)")

        Self.readServerUrl(
            itemServerUrl, 
            ncAccount: ncAccount,
            ncKit: ncKit,
            domain: domain,
            enumeratedItemIdentifier: enumeratedItemIdentifier,
            stopAtMatchingEtags: scanChangesOnly
        ) { metadatas, newMetadatas, updatedMetadatas, deletedMetadatas, readError in

            if readError != nil {
                let nkReadError = NKError(error: readError!)

                // Is the error is that we have found matching etags on this item, then ignore it
                // if we are doing a full rescan
                guard nkReadError.isNoChangesError, scanChangesOnly else {
                    Self.logger.error(
                        "Finishing enumeration of changes at \(itemServerUrl, privacy: .public) with \(readError!.localizedDescription, privacy: .public)"
                    )

                    if nkReadError.isNotFoundError {
                        Self.logger.info(
                            "404 error means item no longer exists. Deleting metadata and reporting as deletion without error"
                        )

                        if let deletedMetadatas =
                            dbManager.deleteDirectoryAndSubdirectoriesMetadata(
                                ocId: directoryMetadata.ocId)
                        {
                            allDeletedMetadatas += deletedMetadatas
                        } else {
                            Self.logger.error(
                                "An error occurred while trying to delete directory and children not found in recursive scan"
                            )
                        }

                    } else if nkReadError.isNoChangesError {  // All is well, just no changed etags
                        Self.logger.info(
                            "Error was to say no changed files -- not bad error. No need to check children."
                        )

                    } else if nkReadError.isUnauthenticatedError
                        || nkReadError.isCouldntConnectError
                    {
                        // If it is a critical error then stop, if not then continue
                        Self.logger.error(
                            "Error will affect next enumerated items, so stopping enumeration.")
                        criticalError = nkReadError
                    }

                    dispatchGroup.leave()
                    return
                }
            }

            Self.logger.info(
                "Finished reading serverUrl: \(itemServerUrl, privacy: .public) for user: \(ncAccount.ncKitAccount, privacy: .public)"
            )

            if let metadatas {
                allMetadatas += metadatas
            } else {
                Self.logger.warning(
                    "WARNING: Nil metadatas received for reading of changes at \(itemServerUrl, privacy: .public) for user: \(ncAccount.ncKitAccount, privacy: .public)"
                )
            }

            if let newMetadatas {
                allNewMetadatas += newMetadatas
            } else {
                Self.logger.warning(
                    "WARNING: Nil new metadatas received for reading of changes at \(itemServerUrl, privacy: .public) for user: \(ncAccount.ncKitAccount, privacy: .public)"
                )
            }

            if let updatedMetadatas {
                allUpdatedMetadatas += updatedMetadatas
            } else {
                Self.logger.warning(
                    "WARNING: Nil updated metadatas received for reading of changes at \(itemServerUrl, privacy: .public) for user: \(ncAccount.ncKitAccount, privacy: .public)"
                )
            }

            if let deletedMetadatas {
                allDeletedMetadatas += deletedMetadatas
            } else {
                Self.logger.warning(
                    "WARNING: Nil deleted metadatas received for reading of changes at \(itemServerUrl, privacy: .public) for user: \(ncAccount.ncKitAccount, privacy: .public)"
                )
            }

            dispatchGroup.leave()
        }

        dispatchGroup.wait()

        guard criticalError == nil else {
            Self.logger.error(
                "Received critical error stopping further scanning: \(criticalError!.errorDescription, privacy: .public)"
            )
            return ([], [], [], [], error: criticalError)
        }

        var childDirectoriesToScan: [ItemMetadata] = []
        var candidateMetadatas: [ItemMetadata]

        if scanChangesOnly, fastEnumeration {
            candidateMetadatas = allUpdatedMetadatas
        } else if scanChangesOnly {
            candidateMetadatas = allUpdatedMetadatas + allNewMetadatas
        } else {
            candidateMetadatas = allMetadatas
        }

        for candidateMetadata in candidateMetadatas {
            if candidateMetadata.directory {
                childDirectoriesToScan.append(candidateMetadata)
            }
        }

        Self.logger.debug("Candidate metadatas for further scan: \(candidateMetadatas, privacy: .public)")

        if childDirectoriesToScan.isEmpty {
            return (
                metadatas: allMetadatas, newMetadatas: allNewMetadatas,
                updatedMetadatas: allUpdatedMetadatas, deletedMetadatas: allDeletedMetadatas, nil
            )
        }

        for childDirectory in childDirectoriesToScan {
            Self.logger.debug(
                "About to recursively scan: \(childDirectory.urlBase, privacy: .public) with etag: \(childDirectory.etag, privacy: .public)"
            )
            let childScanResult = scanRecursively(
                childDirectory, ncAccount: ncAccount, ncKit: ncKit, scanChangesOnly: scanChangesOnly
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

    static func handleDepth1ReadFileOrFolder(
        serverUrl: String,
        ncAccount: Account,
        files: [NKFile],
        error: NKError,
        completionHandler: @escaping (
            _ metadatas: [ItemMetadata]?,
            _ newMetadatas: [ItemMetadata]?,
            _ updatedMetadatas: [ItemMetadata]?,
            _ deletedMetadatas: [ItemMetadata]?,
            _ readError: Error?
        ) -> Void
    ) {
        guard error == .success else {
            Self.logger.error(
                "1 depth readFileOrFolder of url: \(serverUrl, privacy: .public) did not complete successfully, received error: \(error.errorDescription, privacy: .public)"
            )
            completionHandler(nil, nil, nil, nil, error.error)
            return
        }

        Self.logger.debug(
            "Starting async conversion of NKFiles for serverUrl: \(serverUrl, privacy: .public) for user: \(ncAccount.ncKitAccount, privacy: .public)"
        )

        let dbManager = FilesDatabaseManager.shared

        DispatchQueue.global(qos: .userInitiated).async {
            ItemMetadata.metadatasFromDirectoryReadNKFiles(
                files, account: ncAccount.ncKitAccount
            ) { directoryMetadata, _, metadatas in

                // STORE DATA FOR CURRENTLY SCANNED DIRECTORY
                // We have now scanned this directory's contents, so update with etag in order to not check again if not needed
                // unless it's the root container
                if serverUrl != ncAccount.davFilesUrl {
                    dbManager.addItemMetadata(directoryMetadata)
                }

                // Don't update the etags for folders as we haven't checked their contents.
                // When we do a recursive check, if we update the etags now, we will think
                // that our local copies are up to date -- instead, leave them as the old.
                // They will get updated when they are the subject of a readServerUrl call.
                // (See above)
                let changedMetadatas = dbManager.updateItemMetadatas(
                    account: ncAccount.ncKitAccount, serverUrl: serverUrl,
                    updatedMetadatas: metadatas,
                    updateDirectoryEtags: false)

                DispatchQueue.main.async {
                    completionHandler(
                        metadatas, changedMetadatas.newMetadatas, changedMetadatas.updatedMetadatas,
                        changedMetadatas.deletedMetadatas, nil)
                }
            }
        }
    }

    static func readServerUrl(
        _ serverUrl: String,
        ncAccount: Account,
        ncKit: NextcloudKit,
        domain: NSFileProviderDomain? = nil,
        enumeratedItemIdentifier: NSFileProviderItemIdentifier? = nil,
        stopAtMatchingEtags: Bool = false,
        depth: String = "1",
        completionHandler: @escaping (
            _ metadatas: [ItemMetadata]?,
            _ newMetadatas: [ItemMetadata]?,
            _ updatedMetadatas: [ItemMetadata]?,
            _ deletedMetadatas: [ItemMetadata]?,
            _ readError: Error?
        ) -> Void
    ) {
        let dbManager = FilesDatabaseManager.shared
        let ncKitAccount = ncAccount.ncKitAccount

        Self.logger.debug(
            "Starting to read serverUrl: \(serverUrl, privacy: .public) for user: \(ncAccount.ncKitAccount, privacy: .public) at depth \(depth, privacy: .public). NCKit info: userId: \(ncKit.nkCommonInstance.user, privacy: .public), password is empty: \(ncKit.nkCommonInstance.password == "" ? "EMPTY PASSWORD" : "NOT EMPTY PASSWORD"), urlBase: \(ncKit.nkCommonInstance.urlBase, privacy: .public), ncVersion: \(ncKit.nkCommonInstance.nextcloudVersion, privacy: .public)"
        )

        ncKit.readFileOrFolder(
            serverUrlFileName: serverUrl,
            depth: depth,
            showHiddenFiles: true,
            taskHandler: { task in
                if let domain, let enumeratedItemIdentifier {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: enumeratedItemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        ) {
            _, files, _, error in
            guard error == .success else {
                Self.logger.error(
                    "\(depth, privacy: .public) depth readFileOrFolder of url: \(serverUrl, privacy: .public) did not complete successfully, received error: \(error.errorDescription, privacy: .public)"
                )
                completionHandler(nil, nil, nil, nil, error.error)
                return
            }

            guard let receivedFile = files.first else {
                Self.logger.error(
                    "Received no items from readFileOrFolder of \(serverUrl, privacy: .public), not much we can do..."
                )
                completionHandler(nil, nil, nil, nil, error.error)
                return
            }

            guard receivedFile.directory else {
                Self.logger.debug(
                    "Read item is a file. Converting NKfile for serverUrl: \(serverUrl, privacy: .public) for user: \(ncAccount.ncKitAccount, privacy: .public)"
                )
                let itemMetadata = ItemMetadata.fromNKFile(
                    receivedFile, account: ncKitAccount)
                dbManager.addItemMetadata(itemMetadata)  // TODO: Return some value when it is an update
                completionHandler([itemMetadata], nil, nil, nil, error.error)
                return
            }

            if stopAtMatchingEtags,
                let directoryMetadata = dbManager.directoryMetadata(
                    account: ncKitAccount, serverUrl: serverUrl)
            {
                let directoryEtag = directoryMetadata.etag

                guard directoryEtag == "" || directoryEtag != receivedFile.etag else {
                    Self.logger.debug(
                        "Read server url called with flag to stop enumerating at matching etags. Returning and providing soft error."
                    )

                    let description =
                        "Fetched directory etag is same as that stored locally. Not fetching child items."
                    let nkError = NKError(
                        errorCode: NKError.noChangesErrorCode, errorDescription: description)

                    let metadatas = dbManager.itemMetadatas(
                        account: ncKitAccount, serverUrl: serverUrl)

                    completionHandler(metadatas, nil, nil, nil, nkError.error)
                    return
                }
            }

            if depth == "0" {
                if serverUrl != ncAccount.davFilesUrl {
                    let metadata = ItemMetadata.fromNKFile(
                        receivedFile, account: ncKitAccount)
                    let isNew = dbManager.itemMetadataFromOcId(metadata.ocId) == nil
                    let updatedMetadatas = isNew ? [] : [metadata]
                    let newMetadatas = isNew ? [metadata] : []

                    dbManager.addItemMetadata(metadata)

                    DispatchQueue.main.async {
                        completionHandler([metadata], newMetadatas, updatedMetadatas, nil, nil)
                    }
                }
            } else {
                handleDepth1ReadFileOrFolder(
                    serverUrl: serverUrl, ncAccount: ncAccount, files: files, error: error,
                    completionHandler: completionHandler)
            }
        }
    }
}

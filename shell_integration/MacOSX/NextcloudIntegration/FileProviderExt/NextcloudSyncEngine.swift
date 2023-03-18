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

class NextcloudSyncEngine : NSObject {

    var isInvalidated = false

    func invalidate() {
        self.isInvalidated = true
    }

    func fullRecursiveScan(ncAccount: NextcloudAccount,
                           ncKit: NextcloudKit,
                           scanChangesOnly: Bool,
                           singleFolderScanCompleteCompletionHandler: @escaping(_ metadatas: [NextcloudItemMetadataTable]?,
                                                                                _ error: NKError?) -> Void,
                           completionHandler: @escaping(_ metadatas: [NextcloudItemMetadataTable],
                                                        _ newMetadatas: [NextcloudItemMetadataTable],
                                                        _ updatedMetadatas: [NextcloudItemMetadataTable],
                                                        _ deletedMetadatas: [NextcloudItemMetadataTable],
                                                        _ error: NKError?) -> Void) {

        let rootContainerDirectoryMetadata = NextcloudItemMetadataTable()
        rootContainerDirectoryMetadata.directory = true
        rootContainerDirectoryMetadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue

        // Create a serial dispatch queue
        let dispatchQueue = DispatchQueue(label: "recursiveChangeEnumerationQueue", qos: .userInitiated)

        dispatchQueue.async {
            let results = self.scanRecursively(rootContainerDirectoryMetadata,
                                               ncAccount: ncAccount,
                                               ncKit: ncKit,
                                               scanChangesOnly: scanChangesOnly,
                                               singleFolderScanCompleteCompletionHandler: singleFolderScanCompleteCompletionHandler)

            // Run a check to ensure files deleted in one location are not updated in another (e.g. when moved)
            // The recursive scan provides us with updated/deleted metadatas only on a folder by folder basis;
            // so we need to check we are not simultaneously marking a moved file as deleted and updated
            var checkedDeletedMetadatas = results.deletedMetadatas

            for updatedMetadata in results.updatedMetadatas {
                guard let matchingDeletedMetadataIdx = checkedDeletedMetadatas.firstIndex(where: { $0.ocId == updatedMetadata.ocId } ) else {
                    continue;
                }

                checkedDeletedMetadatas.remove(at: matchingDeletedMetadataIdx)
            }

            DispatchQueue.main.async {
                completionHandler(results.metadatas, results.newMetadatas, results.updatedMetadatas, checkedDeletedMetadatas, results.error)
            }
        }
    }

    private func scanRecursively(_ directoryMetadata: NextcloudItemMetadataTable,
                                 ncAccount: NextcloudAccount,
                                 ncKit: NextcloudKit,
                                 scanChangesOnly: Bool,
                                 singleFolderScanCompleteCompletionHandler: @escaping(_ metadatas: [NextcloudItemMetadataTable]?,
                                                                                      _ error: NKError?) -> Void) -> (metadatas: [NextcloudItemMetadataTable],
                                                                                                                                               newMetadatas: [NextcloudItemMetadataTable],
                                                                                                                                               updatedMetadatas: [NextcloudItemMetadataTable],
                                                                                                                                               deletedMetadatas: [NextcloudItemMetadataTable],
                                                                                                                                               error: NKError?) {

        if self.isInvalidated {
            DispatchQueue.main.async {
                singleFolderScanCompleteCompletionHandler(nil, nil)
            }
            return ([], [], [], [], nil)
        }

        assert(directoryMetadata.directory, "Can only recursively scan a directory.")

        // Scanned in this directory
        var currentMetadatas: [NextcloudItemMetadataTable] = []

        // Will include results of recursive calls
        var allMetadatas: [NextcloudItemMetadataTable] = []
        var allNewMetadatas: [NextcloudItemMetadataTable] = []
        var allUpdatedMetadatas: [NextcloudItemMetadataTable] = []
        var allDeletedMetadatas: [NextcloudItemMetadataTable] = []

        let dbManager = NextcloudFilesDatabaseManager.shared
        let dispatchGroup = DispatchGroup() // TODO: Maybe own thread?

        dispatchGroup.enter()

        var criticalError: NKError?
        let itemServerUrl = directoryMetadata.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue ?
            ncAccount.davFilesUrl : directoryMetadata.serverUrl + "/" + directoryMetadata.fileName

        Logger.enumeration.debug("About to read: \(itemServerUrl, privacy: OSLogPrivacy.auto(mask: .hash))")

        NextcloudSyncEngine.readServerUrl(itemServerUrl, ncAccount: ncAccount, ncKit: ncKit, stopAtMatchingEtags: scanChangesOnly) { metadatas, newMetadatas, updatedMetadatas, deletedMetadatas, readError in

            if readError != nil {
                let nkReadError = NKError(error: readError!)

                // Is the error is that we have found matching etags on this item, then ignore it
                // if we are doing a full rescan
                guard nkReadError.isNoChangesError && scanChangesOnly else {
                    Logger.enumeration.error("Finishing enumeration of changes at \(itemServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) with \(readError!.localizedDescription, privacy: .public)")

                    if nkReadError.isNotFoundError {
                        Logger.enumeration.info("404 error means item no longer exists. Deleting metadata and reporting as deletion without error")

                        if let deletedMetadatas = dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: directoryMetadata.ocId) {
                            allDeletedMetadatas += deletedMetadatas
                        } else {
                            Logger.enumeration.error("An error occurred while trying to delete directory and children not found in recursive scan")
                        }

                    } else if nkReadError.isNoChangesError { // All is well, just no changed etags
                        Logger.enumeration.info("Error was to say no changed files -- not bad error. No need to check children.")

                    } else if nkReadError.isUnauthenticatedError || nkReadError.isCouldntConnectError {
                        // If it is a critical error then stop, if not then continue
                        Logger.enumeration.error("Error will affect next enumerated items, so stopping enumeration.")
                        criticalError = nkReadError

                    }

                    dispatchGroup.leave()
                    return
                }
            }

            Logger.enumeration.info("Finished reading serverUrl: \(itemServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")

            if let metadatas = metadatas {
                currentMetadatas = metadatas
                allMetadatas += metadatas
            } else {
                Logger.enumeration.warning("WARNING: Nil metadatas received for reading of changes at \(itemServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")
            }

            if let newMetadatas = newMetadatas {
                allNewMetadatas += newMetadatas
            } else {
                Logger.enumeration.warning("WARNING: Nil new metadatas received for reading of changes at \(itemServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")
            }

            if let updatedMetadatas = updatedMetadatas {
                allUpdatedMetadatas += updatedMetadatas
            } else {
                Logger.enumeration.warning("WARNING: Nil updated metadatas received for reading of changes at \(itemServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")
            }

            if let deletedMetadatas = deletedMetadatas {
                allDeletedMetadatas += deletedMetadatas
            } else {
                Logger.enumeration.warning("WARNING: Nil deleted metadatas received for reading of changes at \(itemServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")
            }

            dispatchGroup.leave()
        }

        dispatchGroup.wait()

        guard criticalError == nil else {
            DispatchQueue.main.async {
                singleFolderScanCompleteCompletionHandler(nil, criticalError)
            }
            return ([], [], [], [], error: criticalError)
        }

        DispatchQueue.main.async {
            singleFolderScanCompleteCompletionHandler(currentMetadatas, nil)
        }

        var childDirectoriesToScan: [NextcloudItemMetadataTable] = []
        var candidateMetadatas: [NextcloudItemMetadataTable]

        if scanChangesOnly {
            candidateMetadatas = allUpdatedMetadatas
        } else {
            candidateMetadatas = allMetadatas
        }

        for candidateMetadata in candidateMetadatas {
            if candidateMetadata.directory {
                childDirectoriesToScan.append(candidateMetadata)
            }
        }

        if childDirectoriesToScan.isEmpty {
            return (metadatas: allMetadatas, newMetadatas: allNewMetadatas, updatedMetadatas: allUpdatedMetadatas, deletedMetadatas: allDeletedMetadatas, nil)
        }

        for childDirectory in childDirectoriesToScan {
            let childScanResult = scanRecursively(childDirectory,
                                                  ncAccount: ncAccount,
                                                  ncKit: ncKit,
                                                  scanChangesOnly: scanChangesOnly,
                                                  singleFolderScanCompleteCompletionHandler: singleFolderScanCompleteCompletionHandler)

            allMetadatas += childScanResult.metadatas
            allNewMetadatas += childScanResult.newMetadatas
            allUpdatedMetadatas += childScanResult.updatedMetadatas
            allDeletedMetadatas += childScanResult.deletedMetadatas
        }

        return (metadatas: allMetadatas, newMetadatas: allNewMetadatas, updatedMetadatas: allUpdatedMetadatas, deletedMetadatas: allDeletedMetadatas, nil)
    }

    static func readServerUrl(_ serverUrl: String, ncAccount: NextcloudAccount, ncKit: NextcloudKit, stopAtMatchingEtags: Bool = false, completionHandler: @escaping (_ metadatas: [NextcloudItemMetadataTable]?, _ newMetadatas: [NextcloudItemMetadataTable]?, _ updatedMetadatas: [NextcloudItemMetadataTable]?, _ deletedMetadatas: [NextcloudItemMetadataTable]?, _ readError: Error?) -> Void) {
        let dbManager = NextcloudFilesDatabaseManager.shared
        let ncKitAccount = ncAccount.ncKitAccount

        Logger.enumeration.debug("Starting to read serverUrl: \(serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) at depth 0. NCKit info: userId: \(ncKit.nkCommonInstance.user), password: \(ncKit.nkCommonInstance.password == "" ? "EMPTY PASSWORD" : "NOT EMPTY PASSWORD"), urlBase: \(ncKit.nkCommonInstance.urlBase), ncVersion: \(ncKit.nkCommonInstance.nextcloudVersion)")

        ncKit.readFileOrFolder(serverUrlFileName: serverUrl, depth: "0", showHiddenFiles: true) { account, files, _, error in
            guard error == .success else {
                Logger.enumeration.error("0 depth readFileOrFolder of url: \(serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) did not complete successfully, received error: \(error.errorDescription, privacy: .public)")
                completionHandler(nil, nil, nil, nil, error.error)
                return
            }

            guard let receivedItem = files.first else {
                Logger.enumeration.error("Received no items from readFileOrFolder of \(serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)), not much we can do...")
                completionHandler(nil, nil, nil, nil, error.error)
                return
            }

            guard receivedItem.directory else {
                Logger.enumeration.debug("Read item is a file. Converting NKfile for serverUrl: \(serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")
                let itemMetadata = dbManager.convertNKFileToItemMetadata(receivedItem, account: ncKitAccount)
                dbManager.addItemMetadata(itemMetadata) // TODO: Return some value when it is an update
                completionHandler([itemMetadata], nil, nil, nil, error.error)
                return
            }

            if stopAtMatchingEtags,
               let directoryMetadata = dbManager.directoryMetadata(account: ncKitAccount, serverUrl: serverUrl) {

                let directoryEtag = directoryMetadata.etag

                guard directoryEtag == "" || directoryEtag != receivedItem.etag else {
                    Logger.enumeration.debug("Read server url called with flag to stop enumerating at matching etags. Returning and providing soft error.")

                    let description = "Fetched directory etag is same as that stored locally. Not fetching child items."
                    let nkError = NKError(errorCode: NKError.noChangesErrorCode, errorDescription: description)

                    let metadatas = dbManager.itemMetadatas(account: account, serverUrl: serverUrl)

                    completionHandler(metadatas, nil, nil, nil, nkError.error)
                    return
                }
            }

            Logger.enumeration.debug("Starting to read serverUrl: \(serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) at depth 1")

            ncKit.readFileOrFolder(serverUrlFileName: serverUrl, depth: "1", showHiddenFiles: true) { account, files, _, error in
                guard error == .success else {
                    Logger.enumeration.error("1 depth readFileOrFolder of url: \(serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) did not complete successfully, received error: \(error.errorDescription, privacy: .public)")
                    completionHandler(nil, nil, nil, nil, error.error)
                    return
                }

                Logger.enumeration.debug("Starting async conversion of NKFiles for serverUrl: \(serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")
                DispatchQueue.main.async {
                    dbManager.convertNKFilesFromDirectoryReadToItemMetadatas(files, account: ncKitAccount) { directoryMetadata, childDirectoriesMetadata, metadatas in

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
                        let changedMetadatas = dbManager.updateItemMetadatas(account: ncKitAccount, serverUrl: serverUrl, updatedMetadatas: metadatas, updateDirectoryEtags: false)

                        completionHandler(metadatas, changedMetadatas.newMetadatas, changedMetadatas.updatedMetadatas, changedMetadatas.deletedMetadatas, nil)
                    }
                }
            }
        }
    }
}

/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

class FileProviderEnumerator: NSObject, NSFileProviderEnumerator {
    
    private let enumeratedItemIdentifier: NSFileProviderItemIdentifier
    private var enumeratedItemMetadata: NextcloudItemMetadataTable?
    private var enumeratingSystemIdentifier: Bool {
        return FileProviderEnumerator.isSystemIdentifier(enumeratedItemIdentifier)
    }
    private let anchor = NSFileProviderSyncAnchor(Date().description.data(using: .utf8)!) // TODO: actually use this in NCKit and server requests
    private static let maxItemsPerFileProviderPage = 100
    let ncAccount: NextcloudAccount
    let ncKit: NextcloudKit
    var serverUrl: String = ""

    private static func isSystemIdentifier(_ identifier: NSFileProviderItemIdentifier) -> Bool {
        return identifier == .rootContainer ||
            identifier == .trashContainer ||
            identifier == .workingSet
    }
    
    init(enumeratedItemIdentifier: NSFileProviderItemIdentifier, ncAccount: NextcloudAccount, ncKit: NextcloudKit) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        self.ncAccount = ncAccount
        self.ncKit = ncKit

        if FileProviderEnumerator.isSystemIdentifier(enumeratedItemIdentifier) {
            Logger.enumeration.debug("Providing enumerator for a system defined container: \(enumeratedItemIdentifier.rawValue, privacy: .public)")
            self.serverUrl = ncAccount.davFilesUrl
        } else {
            Logger.enumeration.debug("Providing enumerator for item with identifier: \(enumeratedItemIdentifier.rawValue, privacy: .public)")
            let dbManager = NextcloudFilesDatabaseManager.shared

            enumeratedItemMetadata = dbManager.itemMetadataFromFileProviderItemIdentifier(enumeratedItemIdentifier)
            if enumeratedItemMetadata != nil {
                self.serverUrl = enumeratedItemMetadata!.serverUrl + "/" + enumeratedItemMetadata!.fileName
            } else {
                Logger.enumeration.error("Could not find itemMetadata for file with identifier: \(enumeratedItemIdentifier.rawValue, privacy: .public)")
            }
        }

        Logger.enumeration.info("Set up enumerator for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash))")
        super.init()
    }

    func invalidate() {
        // TODO: perform invalidation of server connection if necessary
    }

    // MARK: - Protocol methods

    func enumerateItems(for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage) {
        Logger.enumeration.debug("Received enumerate items request for enumerator with user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash))")
        /*
         - inspect the page to determine whether this is an initial or a follow-up request (TODO)
         
         If this is an enumerator for a directory, the root container or all directories:
         - perform a server request to fetch directory contents
         If this is an enumerator for the working set:
         - perform a server request to update your local database
         - fetch the working set from your local database
         
         - inform the observer about the items returned by the server (possibly multiple times)
         - inform the observer that you are finished with this page
         */

        let dbManager = NextcloudFilesDatabaseManager.shared

        // If we don't have any items in the database, ignore this and go for a normal serverUrl read.
        // By default we set the serverUrl to be the webdav files root when we are provided a system container id.
        // However, if we do have items, we want to do a recursive scan of all the folders in the server that
        // ** we have already explored ** . This is to not kill the server.
        if enumeratedItemIdentifier == .workingSet && dbManager.anyItemMetadatasForAccount(ncAccount.ncKitAccount) {
            if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage ||
                page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage {


                let directoryMetadatas = dbManager.directoryMetadatas(account: ncAccount.ncKitAccount)
                var allMetadatas: [NextcloudItemMetadataTable] = []

                var serverError: NKError?

                // Create a serial dispatch queue
                let dispatchQueue = DispatchQueue(label: "workingSetItemEnumerationQueue", qos: .userInitiated)
                let dispatchGroup = DispatchGroup() // To notify when all network tasks are done

                dispatchGroup.notify(queue: DispatchQueue.main) {
                    guard serverError == nil else {
                        observer.finishEnumeratingWithError(serverError!.error)
                        return
                    }

                    FileProviderEnumerator.completeEnumerationObserver(observer, ncKit: self.ncKit, numPage: 1, itemMetadatas: allMetadatas)
                }

                for directoryMetadata in directoryMetadatas {
                    dispatchGroup.enter() // Add to outer counter

                    dispatchQueue.async {
                        let directoryServerUrl = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName

                        guard serverError == nil else {
                            Logger.enumeration.info("Skipping enumeration of directory for working set: \(directoryServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) as we have an error.")
                            dispatchGroup.leave()
                            return;
                        }

                        let currentNetworkTaskDispatchGroup = DispatchGroup() // To make this serial queue wait until this task is done
                        currentNetworkTaskDispatchGroup.enter()

                        FileProviderEnumerator.readServerUrl(directoryServerUrl, ncAccount: self.ncAccount, ncKit: self.ncKit) { metadatas, _, _, _, readError in
                            guard readError == nil else {
                                Logger.enumeration.error("Finishing enumeration of working set directory \(directoryServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) with error \(readError!.localizedDescription, privacy: .public)")

                                let nkError = NKError(error: readError!)
                                if nkError.isUnauthenticatedError || nkError.isCouldntConnectError {
                                    // If it is a critical error then stop, if not then continue
                                    Logger.enumeration.error("Error will affect next enumerated items, so stopping enumeration.")
                                    serverError = nkError
                                }

                                currentNetworkTaskDispatchGroup.leave()
                                return
                            }

                            if let metadatas = metadatas {
                                allMetadatas += metadatas
                            } else {
                                allMetadatas += dbManager.itemMetadatas(account: self.ncAccount.ncKitAccount, serverUrl: directoryServerUrl)
                            }

                            currentNetworkTaskDispatchGroup.leave()
                        }

                        currentNetworkTaskDispatchGroup.wait()
                        dispatchGroup.leave() // Now lower outer counter
                    }
                }

                return
            } else {
                Logger.enumeration.debug("Enumerating page \(page.rawValue) of working set for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")
                // TODO!

                observer.finishEnumerating(upTo: nil)
            }

            return
        } else if enumeratedItemIdentifier == .trashContainer {
            Logger.enumeration.debug("Enumerating trash set for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash))")
            // TODO!

            observer.finishEnumerating(upTo: nil)
            return
        }

        guard serverUrl != "" else {
            Logger.enumeration.error("Enumerator has empty serverUrl -- can't enumerate that! For identifier: \(self.enumeratedItemIdentifier.rawValue, privacy: .public)")
            observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
            return
        }

        // TODO: Make better use of pagination and andle paging properly
        if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage ||
            page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage {

            Logger.enumeration.debug("Enumerating initial page for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash))")

            FileProviderEnumerator.readServerUrl(serverUrl, ncAccount: ncAccount, ncKit: ncKit) { _, _, _, _, readError in

                guard readError == nil else {
                    Logger.enumeration.error("Finishing enumeration for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) with error \(readError!.localizedDescription, privacy: .public)")

                    let nkReadError = NKError(error: readError!)
                    observer.finishEnumeratingWithError(nkReadError.toFileProviderError())
                    return
                }

                let ncKitAccount = self.ncAccount.ncKitAccount

                // Return all now known metadatas
                var metadatas: [NextcloudItemMetadataTable]

                if self.enumeratingSystemIdentifier || (self.enumeratedItemMetadata != nil && self.enumeratedItemMetadata!.directory) {
                    metadatas = NextcloudFilesDatabaseManager.shared.itemMetadatas(account: ncKitAccount, serverUrl: self.serverUrl)
                } else if (self.enumeratedItemMetadata != nil) {
                    guard let updatedEnumeratedItemMetadata = NextcloudFilesDatabaseManager.shared.itemMetadataFromOcId(self.enumeratedItemMetadata!.ocId) else {
                        Logger.enumeration.error("Could not finish enumeration for user: \(ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) as the enumerated item could not be fetched from database. \(self.enumeratedItemIdentifier.rawValue, privacy: .public)")
                        observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
                        return
                    }

                    metadatas = [updatedEnumeratedItemMetadata]
                } else { // We need to have an enumeratedItemMetadata to have a non empty serverUrl
                    Logger.enumeration.error("Cannot finish enumeration for user: \(ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) as we do not have a valid server URL. NOTE: this error should not be possible and indicates something is going wrong before.")
                    observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
                    return
                }

                Logger.enumeration.info("Finished reading serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)). Processed \(metadatas.count) metadatas")

                FileProviderEnumerator.completeEnumerationObserver(observer, ncKit: self.ncKit, numPage: 1, itemMetadatas: metadatas)
            }

            return;
        }

        let numPage = Int(String(data: page.rawValue, encoding: .utf8)!)!
        Logger.enumeration.debug("Enumerating page \(numPage, privacy: .public) for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash))")
        // TODO: Handle paging properly
        // FileProviderEnumerator.completeObserver(observer, ncKit: ncKit, numPage: numPage, itemMetadatas: nil)
        observer.finishEnumerating(upTo: nil)
    }
    
    func enumerateChanges(for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor) {
        Logger.enumeration.debug("Received enumerate changes request for enumerator for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash))")
        /*
         - query the server for updates since the passed-in sync anchor (TODO)
         
         If this is an enumerator for the working set:
         - note the changes in your local database
         
         - inform the observer about item deletions and updates (modifications + insertions)
         - inform the observer when you have finished enumerating up to a subsequent sync anchor
         */

        if enumeratedItemIdentifier == .workingSet {
            Logger.enumeration.debug("Enumerating changes in working set for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")

            FileProviderEnumerator.fullRecursiveScan(ncAccount: self.ncAccount, ncKit: self.ncKit, scanChangesOnly: true) { newMetadatas, updatedMetadatas, deletedMetadatas in

                Logger.enumeration.info("Finished recursive change enumeration of working set for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)). Enumerating items.")

                // Run a check to ensure files deleted in one location are not updated in another (e.g. when moved)
                // The recursive scan provides us with updated/deleted metadatas only on a folder by folder basis;
                // so we need to check we are not simultaneously marking a moved file as deleted and updated
                var checkedDeletedMetadatas = deletedMetadatas

                for updatedMetadata in updatedMetadatas {
                    guard let matchingDeletedMetadataIdx = checkedDeletedMetadatas.firstIndex(where: { $0.ocId == updatedMetadata.ocId } ) else {
                        continue;
                    }

                    checkedDeletedMetadatas.remove(at: matchingDeletedMetadataIdx)
                }

                FileProviderEnumerator.completeChangesObserver(observer,
                                                               anchor: anchor,
                                                               ncKit: self.ncKit,
                                                               newMetadatas: newMetadatas,
                                                               updatedMetadatas: updatedMetadatas,
                                                               deletedMetadatas: checkedDeletedMetadatas)
            }
            return
        } else if enumeratedItemIdentifier == .trashContainer {
            Logger.enumeration.debug("Enumerating changes in trash set for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")
            // TODO!

            observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
            return
        }

        Logger.enumeration.info("Enumerating changes for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash))")

        // No matter what happens here we finish enumeration in some way, either from the error
        // handling below or from the completeChangesObserver
        FileProviderEnumerator.readServerUrl(serverUrl, ncAccount: ncAccount, ncKit: ncKit, stopAtMatchingEtags: true) { _, newMetadatas, updatedMetadatas, deletedMetadatas, readError in

            // If we get a 404 we might add more deleted metadatas
            var currentDeletedMetadatas: [NextcloudItemMetadataTable] = []
            if let notNilDeletedMetadatas = deletedMetadatas {
                currentDeletedMetadatas = notNilDeletedMetadatas
            }

            guard readError == nil else {
                Logger.enumeration.error("Finishing enumeration of changes for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash)) with serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) with error: \(readError!.localizedDescription, privacy: .public)")

                let nkReadError = NKError(error: readError!)
                let fpError = nkReadError.toFileProviderError()

                if nkReadError.isNotFoundError {
                    Logger.enumeration.info("404 error means item no longer exists. Deleting metadata and reporting \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) as deletion without error")

                    guard let itemMetadata = self.enumeratedItemMetadata else {
                        Logger.enumeration.error("Invalid enumeratedItemMetadata, could not delete metadata nor report deletion")
                        observer.finishEnumeratingWithError(fpError)
                        return
                    }

                    let dbManager = NextcloudFilesDatabaseManager.shared
                    if itemMetadata.directory {
                        if let deletedDirectoryMetadatas = dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: itemMetadata.ocId) {
                            currentDeletedMetadatas += deletedDirectoryMetadatas
                        } else {
                            Logger.enumeration.error("Something went wrong when recursively deleting directory not found.")
                        }
                    } else {
                        dbManager.deleteItemMetadata(ocId: itemMetadata.ocId)
                    }

                    FileProviderEnumerator.completeChangesObserver(observer, anchor: anchor, ncKit: self.ncKit, newMetadatas: nil, updatedMetadatas: nil, deletedMetadatas: [itemMetadata])
                    return
                } else if nkReadError.isNoChangesError { // All is well, just no changed etags
                    Logger.enumeration.info("Error was to say no changed files -- not bad error. Finishing change enumeration.")
                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
                    return;
                }

                observer.finishEnumeratingWithError(fpError)
                return
            }

            Logger.enumeration.info("Finished reading serverUrl: \(self.serverUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(self.ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")

            FileProviderEnumerator.completeChangesObserver(observer, anchor: anchor, ncKit: self.ncKit, newMetadatas: newMetadatas, updatedMetadatas: updatedMetadatas, deletedMetadatas: deletedMetadatas)
        }
    }

    func currentSyncAnchor(completionHandler: @escaping (NSFileProviderSyncAnchor?) -> Void) {
        completionHandler(anchor)
    }

    // MARK: - Helper methods

    private static func completeEnumerationObserver(_ observer: NSFileProviderEnumerationObserver, ncKit: NextcloudKit, numPage: Int, itemMetadatas: [NextcloudItemMetadataTable]) {

        var items: [NSFileProviderItem] = []

        for itemMetadata in itemMetadatas {
            if itemMetadata.e2eEncrypted {
                Logger.enumeration.info("Skipping encrypted metadata in enumeration: \(itemMetadata.ocId) \(itemMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
                continue
            }

            if let parentItemIdentifier = NextcloudFilesDatabaseManager.shared.parentItemIdentifierFromMetadata(itemMetadata) {
                let item = FileProviderItem(metadata: itemMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: ncKit)
                Logger.enumeration.debug("Will enumerate item with ocId: \(itemMetadata.ocId) and name: \(itemMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
                items.append(item)
            } else {
                Logger.enumeration.error("Could not get valid parentItemIdentifier for item with ocId: \(itemMetadata.ocId, privacy: .public) and name: \(itemMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash)), skipping enumeration")
            }
        }

        observer.didEnumerate(items)
        Logger.enumeration.info("Did enumerate \(items.count) items")

        // TODO: Handle paging properly
        /*
        if items.count == maxItemsPerFileProviderPage {
            let nextPage = numPage + 1
            let providerPage = NSFileProviderPage("\(nextPage)".data(using: .utf8)!)
            observer.finishEnumerating(upTo: providerPage)
        } else {
            observer.finishEnumerating(upTo: nil)
        }
         */
        observer.finishEnumerating(upTo: NSFileProviderPage("\(numPage)".data(using: .utf8)!))
    }

    private static func completeChangesObserver(_ observer: NSFileProviderChangeObserver, anchor: NSFileProviderSyncAnchor, ncKit: NextcloudKit, newMetadatas: [NextcloudItemMetadataTable]?, updatedMetadatas: [NextcloudItemMetadataTable]?, deletedMetadatas: [NextcloudItemMetadataTable]?) {

        guard newMetadatas != nil || updatedMetadatas != nil || deletedMetadatas != nil else {
            Logger.enumeration.error("Received invalid newMetadatas, updatedMetadatas or deletedMetadatas. Finished enumeration of changes with error.")
            observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
            return
        }

        // Observer does not care about new vs updated, so join
        var allUpdatedMetadatas: [NextcloudItemMetadataTable] = []
        var allDeletedMetadatas: [NextcloudItemMetadataTable] = []

        if let newMetadatas = newMetadatas {
            allUpdatedMetadatas += newMetadatas
        }

        if let updatedMetadatas = updatedMetadatas {
            allUpdatedMetadatas += updatedMetadatas
        }

        if let deletedMetadatas = deletedMetadatas {
            allDeletedMetadatas = deletedMetadatas
        }

        var allFpItemUpdates: [FileProviderItem] = []
        var allFpItemDeletionsIdentifiers = Array(allDeletedMetadatas.map { NSFileProviderItemIdentifier($0.ocId) })

        for updMetadata in allUpdatedMetadatas {
            guard let parentItemIdentifier = NextcloudFilesDatabaseManager.shared.parentItemIdentifierFromMetadata(updMetadata) else {
                Logger.enumeration.warning("Not enumerating change for metadata: \(updMetadata.ocId) \(updMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash)) as could not get parent item metadata.")
                continue
            }

            guard !updMetadata.e2eEncrypted else {
                // Precaution, if all goes well in NKFile conversion then this should not happen
                // TODO: Remove when E2EE supported
                Logger.enumeration.info("Encrypted metadata in changes enumeration \(updMetadata.ocId) \(updMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash)), adding to deletions")
                allFpItemDeletionsIdentifiers.append(NSFileProviderItemIdentifier(updMetadata.ocId))
                continue
            }

            let fpItem = FileProviderItem(metadata: updMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: ncKit)
            allFpItemUpdates.append(fpItem)
        }

        if !allFpItemUpdates.isEmpty {
            observer.didUpdate(allFpItemUpdates)
        }

        if !allFpItemDeletionsIdentifiers.isEmpty {
            observer.didDeleteItems(withIdentifiers: allFpItemDeletionsIdentifiers)
        }

        Logger.enumeration.info("Processed \(allUpdatedMetadatas.count) new or updated metadatas, \(allDeletedMetadatas.count) deleted metadatas.")
        observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
    }

    private static func fullRecursiveScan(ncAccount: NextcloudAccount, ncKit: NextcloudKit, scanChangesOnly: Bool, completionHandler: @escaping(_ newMetadatas: [NextcloudItemMetadataTable], _ updatedMetadatas: [NextcloudItemMetadataTable], _ deletedMetadatas: [NextcloudItemMetadataTable]) -> Void) {

        let rootContainerDirectoryMetadata = NextcloudItemMetadataTable()
        rootContainerDirectoryMetadata.directory = true
        rootContainerDirectoryMetadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue

        // Create a serial dispatch queue
        let dispatchQueue = DispatchQueue(label: "recursiveChangeEnumerationQueue", qos: .background)

        dispatchQueue.async {
            let results = scanRecursively(rootContainerDirectoryMetadata, ncAccount: ncAccount, ncKit: ncKit, scanChangesOnly: scanChangesOnly)

            DispatchQueue.main.async {
                completionHandler(results.newMetadatas, results.updatedMetadatas, results.deletedMetadatas)
            }
        }
    }

    private static func scanRecursively(_ directoryMetadata: NextcloudItemMetadataTable, ncAccount: NextcloudAccount, ncKit: NextcloudKit, scanChangesOnly: Bool) -> (metadatas: [NextcloudItemMetadataTable], newMetadatas: [NextcloudItemMetadataTable], updatedMetadatas: [NextcloudItemMetadataTable], deletedMetadatas: [NextcloudItemMetadataTable]) {

        assert(directoryMetadata.directory, "Can only recursively scan a directory.")

        var allMetadatas: [NextcloudItemMetadataTable] = []
        var allNewMetadatas: [NextcloudItemMetadataTable] = []
        var allUpdatedMetadatas: [NextcloudItemMetadataTable] = []
        var allDeletedMetadatas: [NextcloudItemMetadataTable] = []

        let dbManager = NextcloudFilesDatabaseManager.shared
        let dispatchGroup = DispatchGroup() // TODO: Maybe own thread?

        dispatchGroup.enter()

        let itemServerUrl = directoryMetadata.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue ?
            ncAccount.davFilesUrl : directoryMetadata.serverUrl + "/" + directoryMetadata.fileName

        Logger.enumeration.debug("About to read: \(itemServerUrl, privacy: OSLogPrivacy.auto(mask: .hash))")

        FileProviderEnumerator.readServerUrl(itemServerUrl, ncAccount: ncAccount, ncKit: ncKit, stopAtMatchingEtags: scanChangesOnly) { metadatas, newMetadatas, updatedMetadatas, deletedMetadatas, readError in

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
                    }

                    dispatchGroup.leave()
                    return
                }
            }

            Logger.enumeration.info("Finished reading serverUrl: \(itemServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) for user: \(ncAccount.ncKitAccount, privacy: OSLogPrivacy.auto(mask: .hash))")

            if let metadatas = metadatas {
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

        var updatedDirectories: [NextcloudItemMetadataTable] = []
        for updatedMetadata in allUpdatedMetadatas {
            if updatedMetadata.directory {
                updatedDirectories.append(updatedMetadata)
            }
        }

        if updatedDirectories.isEmpty {
            return (metadatas: allMetadatas, newMetadatas: allNewMetadatas, updatedMetadatas: allUpdatedMetadatas, deletedMetadatas: allDeletedMetadatas)
        }

        for childDirectory in updatedDirectories {
            let childScanResult = scanRecursively(childDirectory, ncAccount: ncAccount, ncKit: ncKit, scanChangesOnly: scanChangesOnly)

            allMetadatas += childScanResult.metadatas
            allNewMetadatas += childScanResult.newMetadatas
            allUpdatedMetadatas += childScanResult.updatedMetadatas
            allDeletedMetadatas += childScanResult.deletedMetadatas
        }

        return (metadatas: allMetadatas, newMetadatas: allNewMetadatas, updatedMetadatas: allUpdatedMetadatas, deletedMetadatas: allDeletedMetadatas)
    }

    private static func readServerUrl(_ serverUrl: String, ncAccount: NextcloudAccount, ncKit: NextcloudKit, stopAtMatchingEtags: Bool = false, completionHandler: @escaping (_ metadatas: [NextcloudItemMetadataTable]?, _ newMetadatas: [NextcloudItemMetadataTable]?, _ updatedMetadatas: [NextcloudItemMetadataTable]?, _ deletedMetadatas: [NextcloudItemMetadataTable]?, _ readError: Error?) -> Void) {
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

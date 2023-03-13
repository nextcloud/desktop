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
            NSLog("Providing enumerator for a system defined container: %@", enumeratedItemIdentifier.rawValue)
            self.serverUrl = ncAccount.davFilesUrl
        } else {
            NSLog("Providing enumerator for item with identifier: %@", enumeratedItemIdentifier.rawValue)
            let dbManager = NextcloudFilesDatabaseManager.shared

            enumeratedItemMetadata = dbManager.itemMetadataFromFileProviderItemIdentifier(enumeratedItemIdentifier)
            if enumeratedItemMetadata != nil {
                self.serverUrl = enumeratedItemMetadata!.serverUrl + "/" + enumeratedItemMetadata!.fileName
            } else {
                NSLog("Could not find itemMetadata for file with identifier: %@", enumeratedItemIdentifier.rawValue)
            }
        }

        NSLog("Set up enumerator for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)
        super.init()
    }

    func invalidate() {
        // TODO: perform invalidation of server connection if necessary
    }

    // MARK: - Protocol methods

    func enumerateItems(for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage) {
        NSLog("Received enumerate items request for enumerator with user: %@ with serverUrl: %@", ncAccount.username, serverUrl)
        /* TODO:
         - inspect the page to determine whether this is an initial or a follow-up request
         
         If this is an enumerator for a directory, the root container or all directories:
         - perform a server request to fetch directory contents
         If this is an enumerator for the active set:
         - perform a server request to update your local database
         - fetch the active set from your local database
         
         - inform the observer about the items returned by the server (possibly multiple times)
         - inform the observer that you are finished with this page
         */

        let dbManager = NextcloudFilesDatabaseManager.shared

        if enumeratedItemIdentifier == .workingSet && dbManager.anyItemMetadatasForAccount(ncAccount.ncKitAccount) {
            if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage ||
                page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage {


                let directoryMetadatas = dbManager.directoryMetadatas(account: ncAccount.ncKitAccount)
                var allMetadatas: [NextcloudItemMetadataTable] = []

                let dispatchGroup = DispatchGroup()  // TODO: Maybe own thread?

                for directoryMetadata in directoryMetadatas {
                    guard directoryMetadata.etag != "" else {
                        NSLog("Skipping enumeration of unexplored directory for working set: %@", directoryMetadata.serverUrl)
                        continue;
                    }

                    dispatchGroup.enter()

                    FileProviderEnumerator.readServerUrl(directoryMetadata.serverUrl, ncAccount: ncAccount, ncKit: ncKit) { metadatas, _, _, _, readError in
                        guard readError == nil else {
                            NSLog("Finishing enumeration of working set directory %@ with error %@", directoryMetadata.serverUrl, readError!.localizedDescription)

                            if let nkReadError = readError as? NKError, nkReadError.errorCode == 404 {
                                NSLog("404 error means item no longer exists. Deleting metadata and reporting as deletion without error")
                                dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: directoryMetadata.ocId)
                            }

                            dispatchGroup.leave()
                            return
                        }

                        if let metadatas = metadatas {
                            allMetadatas += metadatas
                        } else {
                            allMetadatas += dbManager.itemMetadatas(account: self.ncAccount.ncKitAccount, serverUrl: directoryMetadata.serverUrl)
                        }

                        dispatchGroup.leave()
                    }

                    dispatchGroup.wait()
                }

                FileProviderEnumerator.completeEnumerationObserver(observer, ncKit: self.ncKit, numPage: 1, itemMetadatas: allMetadatas)
            } else {
                NSLog("Enumerating page of working set for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)
                observer.finishEnumerating(upTo: nil)
            }

            return
        } else if enumeratedItemIdentifier == .trashContainer {
            NSLog("Enumerating trash set for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)
            // TODO!

            observer.finishEnumerating(upTo: nil)
            return
        }

        guard serverUrl != "" else {
            NSLog("Enumerator has empty serverUrl -- can't enumerate that! For identifier: %@", enumeratedItemIdentifier.rawValue)
            observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
            return
        }

        // TODO: Make better use of pagination and andle paging properly
        if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage ||
            page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage {

            NSLog("Enumerating initial page for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)

            FileProviderEnumerator.readServerUrl(serverUrl, ncAccount: ncAccount, ncKit: ncKit) { _, _, _, _, readError in

                guard readError == nil else {
                    NSLog("Finishing enumeration with error")

                    if let nkReadError = readError as? NKError {
                        observer.finishEnumeratingWithError(nkReadError.toFileProviderError())
                    } else {
                        observer.finishEnumeratingWithError(readError!)
                    }

                    return
                }

                let ncKitAccount = self.ncAccount.ncKitAccount

                // Return all now known metadatas
                var metadatas: [NextcloudItemMetadataTable]

                if self.enumeratingSystemIdentifier || (self.enumeratedItemMetadata != nil && self.enumeratedItemMetadata!.directory) {
                    metadatas = NextcloudFilesDatabaseManager.shared.itemMetadatas(account: ncKitAccount, serverUrl: self.serverUrl)
                } else if (self.enumeratedItemMetadata != nil) {
                    guard let updatedEnumeratedItemMetadata = NextcloudFilesDatabaseManager.shared.itemMetadataFromOcId(self.enumeratedItemMetadata!.ocId) else {
                        NSLog("Cannot finish enumeration as the enumerated item could not be fetched from database. %@ %@", self.enumeratedItemIdentifier.rawValue, self.serverUrl)
                        observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
                        return
                    }

                    metadatas = [updatedEnumeratedItemMetadata]
                } else {
                    NSLog("Cannot finish enumeration as we do not have a valid server URL. NOTE: this error should not be possible and indicates something is going wrong before.")
                    observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
                    return
                }

                NSLog("Finished reading serverUrl: %@ for user: %@. Processed %d metadatas", self.serverUrl, ncKitAccount, metadatas.count)

                FileProviderEnumerator.completeEnumerationObserver(observer, ncKit: self.ncKit, numPage: 1, itemMetadatas: metadatas)
            }

            return;
        }

        let numPage = Int(String(data: page.rawValue, encoding: .utf8)!)!
        NSLog("Enumerating page %d for user: %@ with serverUrl: %@", numPage, ncAccount.username, serverUrl)
        // TODO: Handle paging properly
        // FileProviderEnumerator.completeObserver(observer, ncKit: ncKit, numPage: numPage, itemMetadatas: nil)
        observer.finishEnumerating(upTo: nil)
    }
    
    func enumerateChanges(for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor) {
        NSLog("Received enumerate changes request for enumerator with user: %@ with serverUrl: %@", ncAccount.username, serverUrl)
        /*
         - query the server for updates since the passed-in sync anchor
         
         If this is an enumerator for the active set:
         - note the changes in your local database
         
         - inform the observer about item deletions and updates (modifications + insertions)
         - inform the observer when you have finished enumerating up to a subsequent sync anchor
         */

        if enumeratedItemIdentifier == .workingSet {
            NSLog("Enumerating changes in working set for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)

            let scanResults = FileProviderEnumerator.fullRecursiveScanForChanges(ncAccount: self.ncAccount, ncKit: self.ncKit)

            FileProviderEnumerator.completeChangesObserver(observer,
                                                           anchor: anchor,
                                                           ncKit: self.ncKit,
                                                           newMetadatas: scanResults.newMetadatas,
                                                           updatedMetadatas: scanResults.updatedMetadatas,
                                                           deletedMetadatas: scanResults.deletedMetadatas)
            return
        } else if enumeratedItemIdentifier == .trashContainer {
            NSLog("Enumerating changes in trash set for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)
            // TODO!

            observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
            return
        }

        NSLog("Enumerating changes for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)

        // No matter what happens here we finish enumeration in some way, either from the error
        // handling below or from the completeChangesObserver
        FileProviderEnumerator.readServerUrl(serverUrl, ncAccount: ncAccount, ncKit: ncKit, stopAtMatchingEtags: true) { _, newMetadatas, updatedMetadatas, deletedMetadatas, readError in
            guard readError == nil else {
                NSLog("Finishing enumeration of changes with error")

                if let nkReadError = readError as? NKError {
                    let fpError = nkReadError.toFileProviderError()

                    if nkReadError.errorCode == 404 {
                        NSLog("404 error means item no longer exists. Deleting metadata and reporting %@ as deletion without error", self.serverUrl)

                        guard let itemMetadata = self.enumeratedItemMetadata else {
                            NSLog("Invalid enumeratedItemMetadata, could not delete metadata nor report deletion")
                            observer.finishEnumeratingWithError(fpError)
                            return
                        }

                        let dbManager = NextcloudFilesDatabaseManager.shared
                        if itemMetadata.directory {
                            dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: itemMetadata.ocId)
                        } else {
                            dbManager.deleteItemMetadata(ocId: itemMetadata.ocId)
                        }

                        FileProviderEnumerator.completeChangesObserver(observer, anchor: anchor, ncKit: self.ncKit, newMetadatas: nil, updatedMetadatas: nil, deletedMetadatas: [itemMetadata])
                        return
                    } else if nkReadError.isNoChangesError { // All is well, just no changed etags
                        NSLog("Error was to say no changed files -- not bad error. Finishing change enumeration.")
                        observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
                        return;
                    }

                    observer.finishEnumeratingWithError(fpError)
                    return
                }

                observer.finishEnumeratingWithError(readError!)
                return
            }

            NSLog("Finished reading serverUrl: %@ for user: %@", self.serverUrl, self.ncAccount.ncKitAccount)

            FileProviderEnumerator.completeChangesObserver(observer, anchor: anchor, ncKit: self.ncKit, newMetadatas: newMetadatas, updatedMetadatas: updatedMetadatas, deletedMetadatas: deletedMetadatas)
        }
    }

    func currentSyncAnchor(completionHandler: @escaping (NSFileProviderSyncAnchor?) -> Void) {
        completionHandler(anchor)
    }

    // MARK: - Helper methods

    private static func completeEnumerationObserver(_ observer: NSFileProviderEnumerationObserver, ncKit: NextcloudKit, numPage: Int, itemMetadatas: [NextcloudItemMetadataTable], createLocalFileOrDirectory: Bool = true) {

        var items: [NSFileProviderItem] = []

        for itemMetadata in itemMetadatas {
            if itemMetadata.e2eEncrypted {
                NSLog("Skipping encrypted metadata in enumeration")
                continue
            }

            if createLocalFileOrDirectory {
                createFileOrDirectoryLocally(metadata: itemMetadata)
            }

            if let parentItemIdentifier = NextcloudFilesDatabaseManager.shared.parentItemIdentifierFromMetadata(itemMetadata) {
                let item = FileProviderItem(metadata: itemMetadata, parentItemIdentifier: parentItemIdentifier, ncKit: ncKit)
                NSLog("Will enumerate item with ocId: %@ and name: %@", itemMetadata.ocId, itemMetadata.fileName)
                items.append(item)
            } else {
                NSLog("Could not get valid parentItemIdentifier for item with ocId: %@ and name: %@, skipping enumeration", itemMetadata.ocId, itemMetadata.fileName)
            }
        }

        observer.didEnumerate(items)
        NSLog("Did enumerate %d items", items.count)

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
            NSLog("Received invalid newMetadatas, updatedMetadatas or deletedMetadatas. Finished enumeration of changes with error.")
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
                NSLog("Not enumerating change for metadata: %@ %@ as could not get parent item metadata.", updMetadata.ocId, updMetadata.fileName)
                continue
            }

            guard !updMetadata.e2eEncrypted else {
                // Precaution, if all goes well in NKFile conversion then this should not happen
                // TODO: Remove when E2EE supported
                NSLog("Encrypted metadata in changes enumeration, adding to deletions")
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

        NSLog("Processed %d new or updated metadatas, %d deleted metadatas.", allUpdatedMetadatas.count, allDeletedMetadatas.count)
        observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
    }

    private static func fullRecursiveScanForChanges(ncAccount: NextcloudAccount, ncKit: NextcloudKit) -> (newMetadatas: [NextcloudItemMetadataTable], updatedMetadatas: [NextcloudItemMetadataTable], deletedMetadatas: [NextcloudItemMetadataTable]) {

        let rootContainerDirectoryMetadata = NextcloudDirectoryMetadataTable()
        rootContainerDirectoryMetadata.serverUrl = ncAccount.davFilesUrl
        rootContainerDirectoryMetadata.account = ncAccount.ncKitAccount
        rootContainerDirectoryMetadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue

        return scanRecursivelyForChanges(rootContainerDirectoryMetadata, ncAccount: ncAccount, ncKit: ncKit)
    }

    private static func scanRecursivelyForChanges(_ directoryMetadata: NextcloudDirectoryMetadataTable, ncAccount: NextcloudAccount, ncKit: NextcloudKit) -> (newMetadatas: [NextcloudItemMetadataTable], updatedMetadatas: [NextcloudItemMetadataTable], deletedMetadatas: [NextcloudItemMetadataTable]) {

        guard directoryMetadata.etag != "" || directoryMetadata.serverUrl == ncAccount.davFilesUrl else {
            NSLog("Skipping enumeration of changes in unexplored directory for working set: %@", directoryMetadata.serverUrl)
            return ([], [], [])
        }

        var allNewMetadatas: [NextcloudItemMetadataTable] = []
        var allUpdatedMetadatas: [NextcloudItemMetadataTable] = []
        var allDeletedMetadatas: [NextcloudItemMetadataTable] = []

        let dbManager = NextcloudFilesDatabaseManager.shared
        let dispatchGroup = DispatchGroup() // TODO: Maybe own thread?

        dispatchGroup.enter()

        NSLog("About to read: %@", directoryMetadata.serverUrl)
        FileProviderEnumerator.readServerUrl(directoryMetadata.serverUrl, ncAccount: ncAccount, ncKit: ncKit, stopAtMatchingEtags: true) { _, newMetadatas, updatedMetadatas, deletedMetadatas, readError in
            guard readError == nil else {
                NSLog("Finishing enumeration of changes at %@ with error %@", directoryMetadata.serverUrl, readError!.localizedDescription)

                if let nkReadError = readError as? NKError {
                    if nkReadError.errorCode == 404 {
                        NSLog("404 error means item no longer exists. Deleting metadata and reporting as deletion without error")

                        guard let directoryItemMetadata = dbManager.itemMetadataFromOcId(directoryMetadata.ocId) else {
                            NSLog("Can't delete directory properly as item metadata not found...")
                            dispatchGroup.leave()
                            return
                        }

                        dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: directoryMetadata.ocId)
                        allDeletedMetadatas.append(directoryItemMetadata)
                    } else if nkReadError.isNoChangesError { // All is well, just no changed etags
                        NSLog("Error was to say no changed files -- not bad error. No need to check children.")
                    }
                }

                dispatchGroup.leave()
                return
            }

            NSLog("Finished reading serverUrl: %@ for user: %@", directoryMetadata.serverUrl, ncAccount.ncKitAccount)
            if let newMetadatas = newMetadatas {
                allNewMetadatas += newMetadatas
            } else {
                NSLog("WARNING: Nil new metadatas received for reading of changes at %@ for user: %@", directoryMetadata.serverUrl, ncAccount.ncKitAccount)
            }

            if let updatedMetadatas = updatedMetadatas {
                allUpdatedMetadatas += updatedMetadatas
            } else {
                NSLog("WARNING: Nil updated metadatas received for reading of changes at %@ for user: %@", directoryMetadata.serverUrl, ncAccount.ncKitAccount)
            }

            if let deletedMetadatas = deletedMetadatas {
                allDeletedMetadatas += deletedMetadatas
            } else {
                NSLog("WARNING: Nil deleted metadatas received for reading of changes at %@ for user: %@", directoryMetadata.serverUrl, ncAccount.ncKitAccount)
            }

            dispatchGroup.leave()
        }

        dispatchGroup.wait()

        var updatedDirectories: [NextcloudDirectoryMetadataTable] = []
        for updatedMetadata in allUpdatedMetadatas {
            if updatedMetadata.directory {
                guard let directoryMetadata = dbManager.directoryMetadata(ocId: updatedMetadata.ocId) else {
                    NSLog("Could not find matching directory metadata for updated item metadata, cannot scan for updates")
                    continue
                }

                updatedDirectories.append(directoryMetadata)
            }
        }

        if updatedDirectories.isEmpty {
            return (newMetadatas: allNewMetadatas, updatedMetadatas: allUpdatedMetadatas, deletedMetadatas: allDeletedMetadatas)
        }

        for childDirectory in updatedDirectories {
            let childScanResult = scanRecursivelyForChanges(childDirectory, ncAccount: ncAccount, ncKit: ncKit)

            allNewMetadatas += childScanResult.newMetadatas
            allUpdatedMetadatas += childScanResult.updatedMetadatas
            allDeletedMetadatas += childScanResult.deletedMetadatas
        }

        return (newMetadatas: allNewMetadatas, updatedMetadatas: allUpdatedMetadatas, deletedMetadatas: allDeletedMetadatas)
    }

    private static func readServerUrl(_ serverUrl: String, ncAccount: NextcloudAccount, ncKit: NextcloudKit, stopAtMatchingEtags: Bool = false, completionHandler: @escaping (_ metadatas: [NextcloudItemMetadataTable]?, _ newMetadatas: [NextcloudItemMetadataTable]?, _ updatedMetadatas: [NextcloudItemMetadataTable]?, _ deletedMetadatas: [NextcloudItemMetadataTable]?, _ readError: Error?) -> Void) {
        let dbManager = NextcloudFilesDatabaseManager.shared
        let ncKitAccount = ncAccount.ncKitAccount

        NSLog("Starting to read serverUrl: %@ for user: %@ at depth 0. NCKit info: user: %@, userId: %@, password: %@, urlBase: %@, ncVersion: %d", serverUrl, ncKitAccount, ncKit.nkCommonInstance.user, ncKit.nkCommonInstance.userId, ncKit.nkCommonInstance.password, ncKit.nkCommonInstance.urlBase, ncKit.nkCommonInstance.nextcloudVersion)

        ncKit.readFileOrFolder(serverUrlFileName: serverUrl, depth: "0", showHiddenFiles: true) { account, files, _, error in
            guard error == .success else {
                NSLog("0 depth readFileOrFolder of url: %@ did not complete successfully, received error: %@", serverUrl, error.errorDescription)
                completionHandler(nil, nil, nil, nil, error.error)
                return
            }

            guard let receivedItem = files.first else {
                NSLog("Received no items from readFileOrFolder, not much we can do...")
                completionHandler(nil, nil, nil, nil, error.error)
                return
            }

            guard receivedItem.directory else {
                NSLog("Read item is a file. Converting NKfile for serverUrl: %@ for user: %@", serverUrl, ncKitAccount)
                let itemMetadata = dbManager.convertNKFileToItemMetadata(receivedItem, account: ncKitAccount)
                dbManager.addItemMetadata(itemMetadata) // TODO: Return some value when it is an update
                completionHandler([itemMetadata], nil, nil, nil, error.error)
                return
            }

            if stopAtMatchingEtags,
               let directoryMetadata = dbManager.directoryMetadata(account: ncKitAccount, serverUrl: serverUrl) {
                
                let directoryEtag = directoryMetadata.etag

                guard directoryEtag == "" || directoryEtag != receivedItem.etag else {
                    let description = String(format: "Fetched directory etag is same as that stored locally (serverUrl: %@ user: %@). Not fetching child items.", serverUrl, account)
                    let nkError = NKError(errorCode: NKError.noChangesErrorCode, errorDescription: description)
                    completionHandler(nil, nil, nil, nil, nkError.error)
                    return
                }
            }

            NSLog("Starting to read serverUrl: %@ for user: %@ at depth 1", serverUrl, ncKitAccount)

            ncKit.readFileOrFolder(serverUrlFileName: serverUrl, depth: "1", showHiddenFiles: true) { account, files, _, error in
                guard error == .success else {
                    NSLog("1 depth readFileOrFolder of url: %@ did not complete successfully, received error: %@", serverUrl, error.errorDescription)
                    completionHandler(nil, nil, nil, nil, error.error)
                    return
                }

                NSLog("Starting async conversion of NKFiles for serverUrl: %@ for user: %@", serverUrl, ncKitAccount)
                DispatchQueue.global().async {
                    dbManager.convertNKFilesFromDirectoryReadToItemMetadatas(files, account: ncKitAccount) { directoryMetadata, childDirectoriesMetadata, metadatas in

                        // STORE DATA FOR CURRENTLY SCANNED DIRECTORY
                        // We have now scanned this directory's contents, so update with etag in order to not check again if not needed
                        // unless it's the root container
                        if serverUrl != ncAccount.davFilesUrl {
                            let directoryItemMetadata = dbManager.directoryMetadataFromItemMetadata(directoryItemMetadata: directoryMetadata, recordEtag: true)
                            dbManager.addDirectoryMetadata(directoryItemMetadata)
                        }

                        // STORE ETAG-LESS DIRECTORY METADATA FOR CHILD DIRECTORIES
                        // Since we haven't scanned the contents of the child directories, don't record their itemMetadata etags in the directory tables
                        // This will delete database records for directories that we did not get from the readFileOrFolder (indicating they were deleted)
                        // as well as deleting the records for all the children contained by the directories.
                        // TODO: Find a way to detect if files have been moved rather than deleted and change the metadata server urls, move the materialised files
                        dbManager.updateDirectoryMetadatasFromItemMetadatas(account: ncKitAccount, parentDirectoryServerUrl: serverUrl, updatedDirectoryItemMetadatas: childDirectoriesMetadata)
                        // TODO: Notify working set changed if new folders found

                        dbManager.updateItemMetadatas(account: ncKitAccount, serverUrl: serverUrl, updatedMetadatas: metadatas) { newMetadatas, updatedMetadatas, deletedMetadatas in
                            completionHandler(metadatas, newMetadatas, updatedMetadatas, deletedMetadatas, nil)
                        }
                    }
                }
            }
        }
    }
}

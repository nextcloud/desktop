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
            if let itemMetadata = dbManager.itemMetadataFromFileProviderItemIdentifier(enumeratedItemIdentifier) {
                self.serverUrl = itemMetadata.serverUrl + "/" + itemMetadata.fileName
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

        if enumeratedItemIdentifier == .workingSet {
            NSLog("Enumerating working set for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)
            // TODO: Enumerate favourites and other special items

            let materialisedFilesMetadatas = NextcloudFilesDatabaseManager.shared.localFileItemMetadatas(account: ncAccount.ncKitAccount)
            FileProviderEnumerator.completeObserver(observer, ncKit: self.ncKit, numPage: 1, itemMetadatas: materialisedFilesMetadatas, createLocalFileOrDirectory: false)
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

        // TODO: Make better use of pagination
        if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage ||
            page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage {

            NSLog("Enumerating initial page for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)

            FileProviderEnumerator.readServerUrl(serverUrl, ncAccount: ncAccount, ncKit: ncKit) { _, _, _, _, readError in

                guard readError == nil else {
                    NSLog("Finishing enumeration with error")
                    observer.finishEnumeratingWithError(readError!)
                    return;
                }

                let ncKitAccount = self.ncAccount.ncKitAccount

                // Return all now known metadatas
                let metadatas = NextcloudFilesDatabaseManager.shared.itemMetadatas(account: ncKitAccount, serverUrl: self.serverUrl)

                NSLog("Finished reading serverUrl: %@ for user: %@. Processed %d metadatas", self.serverUrl, ncKitAccount, metadatas.count)

                FileProviderEnumerator.completeObserver(observer, ncKit: self.ncKit, numPage: 1, itemMetadatas: metadatas)
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
        /* TODO:
         - query the server for updates since the passed-in sync anchor
         
         If this is an enumerator for the active set:
         - note the changes in your local database
         
         - inform the observer about item deletions and updates (modifications + insertions)
         - inform the observer when you have finished enumerating up to a subsequent sync anchor
         */
        observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
    }

    func currentSyncAnchor(completionHandler: @escaping (NSFileProviderSyncAnchor?) -> Void) {
        completionHandler(anchor)
    }

    // MARK: - Helper methods

    private static func completeObserver(_ observer: NSFileProviderEnumerationObserver, ncKit: NextcloudKit, numPage: Int, itemMetadatas: [NextcloudItemMetadataTable], createLocalFileOrDirectory: Bool = true) {

        var items: [NSFileProviderItem] = []

        for itemMetadata in itemMetadatas {
            if itemMetadata.e2eEncrypted {
                NSLog("Skipping encrypted metadata in enumeration")
                continue
            }

            if createLocalFileOrDirectory {
                createFileOrDirectoryLocally(metadata: itemMetadata)
            }

            if let parentItemIdentifier = parentItemIdentifierFromMetadata(itemMetadata) {
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

    private static func readServerUrl(_ serverUrl: String, ncAccount: NextcloudAccount, ncKit: NextcloudKit, completionHandler: @escaping (_ metadatas: [NextcloudItemMetadataTable]?, _ newMetadatas: [NextcloudItemMetadataTable]?, _ updatedMetadatas: [NextcloudItemMetadataTable]?, _ deletedMetadatas: [NextcloudItemMetadataTable]?, _ readError: Error?) -> Void) {
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
                dbManager.addItemMetadata(itemMetadata)
                completionHandler([itemMetadata], nil, nil, nil, error.error)
                return
            }

            // If we have already done a full readFileOrFolder scan of this folder then it will be in the database.
            // We can check for matching etags and stop here if this is the case, as the state is the same.
            if let directoryMetadata = dbManager.directoryMetadata(account: ncKitAccount, serverUrl: serverUrl) {
                let directoryEtag = directoryMetadata.etag

                guard directoryEtag == "" || directoryEtag != receivedItem.etag else {
                    NSLog("Fetched directory etag is same as that stored locally (serverUrl: %@ user: %@). Not fetching child items.", serverUrl, account)
                    completionHandler(nil, nil, nil, nil, nil)
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
                        // unless it's the root container -- this method deletes metadata for directories under the path that we do not
                        // provide as the updatedDirectoryItemMetadatas, don't do this with root folder or we will purge metadatas wrongly
                        if serverUrl != ncAccount.davFilesUrl {
                            dbManager.updateDirectoryMetadatasFromItemMetadatas(account: ncKitAccount, parentDirectoryServerUrl: serverUrl, updatedDirectoryItemMetadatas: [directoryMetadata], recordEtag: true)
                        }

                        let receivedMetadataChanges = dbManager.updateItemMetadatas(account: ncKitAccount, serverUrl: serverUrl, updatedMetadatas: metadatas)

                        // STORE ETAG-LESS DIRECTORY METADATA FOR CHILD DIRECTORIES
                        // Since we haven't scanned the contents of the child directories, don't record their itemMetadata etags in the directory tables
                        // This will delete database records for directories that we did not get from the readFileOrFolder (indicating they were deleted)
                        // as well as deleting the records for all the children contained by the directories.
                        // TODO: Find a way to detect if files have been moved rather than deleted and change the metadata server urls, move the materialised files
                        dbManager.updateDirectoryMetadatasFromItemMetadatas(account: ncKitAccount, parentDirectoryServerUrl: serverUrl, updatedDirectoryItemMetadatas: childDirectoriesMetadata)

                        DispatchQueue.main.async {
                            completionHandler(metadatas, receivedMetadataChanges.newMetadatas, receivedMetadataChanges.updatedMetadatas, receivedMetadataChanges.deletedMetadatas, nil)
                        }
                    }
                }
            }
        }
    }
}

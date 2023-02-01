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
    private let anchor = NSFileProviderSyncAnchor("an anchor".data(using: .utf8)!)
    private static let maxItemsPerFileProviderPage = 100
    var ncAccount: NextcloudAccount
    var serverUrl: String = ""
    
    init(enumeratedItemIdentifier: NSFileProviderItemIdentifier, ncAccount: NextcloudAccount) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        self.ncAccount = ncAccount

        if enumeratedItemIdentifier == .rootContainer {
            NSLog("Providing enumerator for root container")
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
            // TODO
            observer.finishEnumerating(upTo: nil)
            return
        }

        if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage ||
            page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage {

            NSLog("Enumerating initial page for user: %@ with serverUrl: %@", ncAccount.username, serverUrl)
            FileProviderEnumerator.readServerUrl(serverUrl, ncAccount: ncAccount) { metadatas in
                FileProviderEnumerator.completeObserver(observer, numPage: 1, itemMetadatas: metadatas)
            }

            return;
        }

        let numPage = Int(String(data: page.rawValue, encoding: .utf8)!)!
        NSLog("Enumerating page %d for user: %@ with serverUrl: %@", numPage, ncAccount.username, serverUrl)
        FileProviderEnumerator.completeObserver(observer, numPage: numPage, itemMetadatas: nil)
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

    private static func completeObserver(_ observer: NSFileProviderEnumerationObserver, numPage: Int, itemMetadatas: [NextcloudItemMetadataTable]?) {
        guard itemMetadatas != nil else {
            NSLog("Received nil metadatas, finish empty enumeration")
            observer.finishEnumerating(upTo: nil)
            return

        }

        var items: [NSFileProviderItem] = []

        for itemMetadata in itemMetadatas! {
            if itemMetadata.e2eEncrypted {
                NSLog("Skipping encrypted metadata in enumeration")
                continue
            }

            createFileOrDirectoryLocally(metadata: itemMetadata)

            if let parentItemIdentifier = parentItemIdentifierFromMetadata(itemMetadata) {
                let item = FileProviderItem(metadata: itemMetadata, parentItemIdentifier: parentItemIdentifier)
                NSLog("Will enumerate item with ocId: %@ and name: %@", itemMetadata.ocId, itemMetadata.fileName)
                items.append(item)
            } else {
                NSLog("Could not get valid parentItemIdentifier for item with ocId: %@ and name: %@, skipping enumeration", itemMetadata.ocId, itemMetadata.fileName)
            }
        }

        observer.didEnumerate(items)
        NSLog("Did enumerate %d items", items.count)

        if items.count == maxItemsPerFileProviderPage {
            let nextPage = numPage + 1
            let providerPage = NSFileProviderPage("\(nextPage)".data(using: .utf8)!)
            observer.finishEnumerating(upTo: providerPage)
        } else {
            observer.finishEnumerating(upTo: nil)
        }
    }

    private static func finishReadServerUrl(_ serverUrlPath: String, ncKitAccount: String, completionHandler: @escaping (_ metadatas: [NextcloudItemMetadataTable]?) -> Void) {
        let metadatas = NextcloudFilesDatabaseManager.shared.itemMetadatas(account: ncKitAccount, serverUrl: serverUrlPath)

        NSLog("Finished reading serverUrl: %@ for user: %@. Processed %d metadatas", serverUrlPath, ncKitAccount, metadatas.count)
        completionHandler(metadatas)
    }

    private static func readServerUrl(_ serverUrl: String, ncAccount: NextcloudAccount, completionHandler: @escaping (_ metadatas: [NextcloudItemMetadataTable]?) -> Void) {
        let dbManager = NextcloudFilesDatabaseManager.shared
        let ncKitAccount = ncAccount.ncKitAccount
        var directoryEtag: String?

        if let directoryMetadata = dbManager.directoryMetadata(account: ncKitAccount, serverUrl: serverUrl) {
            directoryEtag = directoryMetadata.etag
        }

        NSLog("Starting to read serverUrl: %@ for user: %@ at depth 0", serverUrl, ncKitAccount)

        NextcloudKit.shared.readFileOrFolder(serverUrlFileName: serverUrl, depth: "0", showHiddenFiles: true) { account, files, _, error in
            guard error == .success else {
                NSLog("0 depth readFileOrFolder of url: %@ did not complete successfully, received error: %@", serverUrl, error.errorDescription)
                finishReadServerUrl(serverUrl, ncKitAccount: ncKitAccount, completionHandler: completionHandler)
                return
            }

            // If we have already done a 0 depth scan of this folder then we might get matching etag
            guard directoryEtag != files.first?.etag else {
                NSLog("Fetched directory etag is same as that stored locally (serverUrl: %@ user: %@). Not fetching child items.", serverUrl, account)
                finishReadServerUrl(serverUrl, ncKitAccount: ncKitAccount, completionHandler: completionHandler)
                return
            }

            NSLog("Starting to read serverUrl: %@ for user: %@ at depth 1", serverUrl, ncKitAccount)

            NextcloudKit.shared.readFileOrFolder(serverUrlFileName: serverUrl, depth: "1", showHiddenFiles: true) { account, files, _, error in
                guard error == .success else {
                    NSLog("1 depth readFileOrFolder of url: %@ did not complete successfully, received error: %@", serverUrl, error.errorDescription)
                    finishReadServerUrl(serverUrl, ncKitAccount: ncKitAccount, completionHandler: completionHandler)
                    return
                }

                NSLog("Starting async conversion of NKFiles for serverUrl: %@ for user: %@", serverUrl, ncKitAccount)
                DispatchQueue.global().async {
                    dbManager.convertNKFilesToItemMetadatas(files, account: ncKitAccount) { directoryMetadata, childDirectoriesMetadata, metadatas in

                        // We have now scanned this directory's contents, so update with etag in order to not check again if not needed
                        dbManager.updateDirectoryMetadatasFromItemMetadatas(account: ncKitAccount, parentDirectoryServerUrl: serverUrl, updatedDirectoryItemMetadatas: [directoryMetadata], recordEtag: true)

                        dbManager.updateItemMetadatas(account: ncKitAccount, serverUrl: serverUrl, updatedMetadatas: metadatas)

                        // Since we haven't scanned the contents of these, don't record their itemMetadata etags in the directory tables
                        dbManager.updateDirectoryMetadatasFromItemMetadatas(account: ncKitAccount, parentDirectoryServerUrl: serverUrl, updatedDirectoryItemMetadatas: childDirectoriesMetadata)

                        finishReadServerUrl(serverUrl, ncKitAccount: ncKitAccount, completionHandler: completionHandler)
                    }
                }
            }
        }
    }
}

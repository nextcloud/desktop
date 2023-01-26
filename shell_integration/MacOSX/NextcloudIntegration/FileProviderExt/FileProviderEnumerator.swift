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
    var ncAccount: NextcloudAccount?
    var serverUrl: String?
    
    init(enumeratedItemIdentifier: NSFileProviderItemIdentifier, ncAccount: NextcloudAccount?) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        self.ncAccount = ncAccount

        if enumeratedItemIdentifier == .rootContainer {
            self.serverUrl = ncAccount?.davFilesUrl
        } else {
            let dbManager = NextcloudFilesDatabaseManager.shared
            if let itemMetadata = dbManager.itemMetadataFromFileProviderItemIdentifier(enumeratedItemIdentifier),
               let itemDirectoryMetadata = dbManager.parentDirectoryMetadataForItem(itemMetadata) {

                self.serverUrl = itemDirectoryMetadata.serverUrl + "/" + itemMetadata.fileName
            }

        }

        super.init()
    }

    func invalidate() {
        // TODO: perform invalidation of server connection if necessary
    }

    // MARK: - Protocol methods

    func enumerateItems(for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage) {
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
            // TODO
            observer.finishEnumerating(upTo: nil)
            return
        }

        guard let serverUrl = serverUrl, let ncAccount = ncAccount else { observer.finishEnumerating(upTo: nil); return }

        if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage ||
            page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage {

            FileProviderEnumerator.readServerUrl(serverUrl, ncAccount: ncAccount) { metadatas in
                FileProviderEnumerator.completeObserver(observer, numPage: 1, itemMetadatas: metadatas)
            }
        } else {
            let numPage = Int(String(data: page.rawValue, encoding: .utf8)!)!
            FileProviderEnumerator.completeObserver(observer, numPage: numPage, itemMetadatas: nil)
        }
    }
    
    func enumerateChanges(for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor) {
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
        guard itemMetadatas != nil else { observer.finishEnumerating(upTo: nil); return }

        var items: [NSFileProviderItem] = []

        for itemMetadata in itemMetadatas! {
            if itemMetadata.e2eEncrypted { continue }

            createFileOrDirectoryLocally(metadata: itemMetadata)

            if let parentItemIdentifier = parentItemIdentifierFromMetadata(itemMetadata) {
                let item = FileProviderItem(metadata: itemMetadata, parentItemIdentifier: parentItemIdentifier)
                items.append(item)
            }
        }

        observer.didEnumerate(items)

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
        completionHandler(metadatas)
    }

    private static func readServerUrl(_ serverUrl: String, ncAccount: NextcloudAccount, completionHandler: @escaping (_ metadatas: [NextcloudItemMetadataTable]?) -> Void) {
        let dbManager = NextcloudFilesDatabaseManager.shared
        let ncKitAccount = ncAccount.ncKitAccount
        var directoryEtag: String?

        if let directoryMetadata = dbManager.directoryMetadata(account: ncKitAccount, serverUrl: serverUrl) {
            directoryEtag = directoryMetadata.etag
        }

        NextcloudKit.shared.readFileOrFolder(serverUrlFileName: serverUrl, depth: "0", showHiddenFiles: true) { account, files, _, error in
            guard directoryEtag != files.first?.etag else {
                finishReadServerUrl(serverUrl, ncKitAccount: ncKitAccount, completionHandler: completionHandler)
                return
            }

            NextcloudKit.shared.readFileOrFolder(serverUrlFileName: serverUrl, depth: "1", showHiddenFiles: true) { account, files, _, error in
                guard error == .success else {
                    finishReadServerUrl(serverUrl, ncKitAccount: ncKitAccount, completionHandler: completionHandler)
                    return
                }

                DispatchQueue.global().async {
                    dbManager.convertNKFilesToItemMetadatas(files, account: ncKitAccount) { _, childDirectoriesMetadata, metadatas in
                        dbManager.updateItemMetadatas(account: ncKitAccount, serverUrl: serverUrl, updatedMetadatas: metadatas)
                        dbManager.updateDirectoryMetadatasFromItemMetadatas(account: ncKitAccount, parentDirectoryServerUrl: serverUrl, updatedDirectoryItemMetadatas: childDirectoriesMetadata)
                        finishReadServerUrl(serverUrl, ncKitAccount: ncKitAccount, completionHandler: completionHandler)
                    }
                }
            }
        }
    }
}

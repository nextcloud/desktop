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

class FileProviderEnumerator: NSObject, NSFileProviderEnumerator {
    
    private let enumeratedItemIdentifier: NSFileProviderItemIdentifier
    private let anchor = NSFileProviderSyncAnchor("an anchor".data(using: .utf8)!)
    private let maxItemsPerFileProviderPage = 100
    var serverUrl: URL?
    
    init(enumeratedItemIdentifier: NSFileProviderItemIdentifier, ncAccount: NextcloudAccount?) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier

        if enumeratedItemIdentifier == .rootContainer {
            self.serverUrl = ncAccount?.davFilesUrl
        } else {
            let dbManager = NextcloudFilesDatabaseManager.shared
            if let itemMetadata = dbManager.itemMetadataFromFileProviderItemIdentifier(enumeratedItemIdentifier),
               let itemDirectoryMetadata = dbManager.parentDirectoryMetadataForItem(itemMetadata) {

                self.serverUrl = URL(string: itemDirectoryMetadata.serverUrl + "/" + itemMetadata.fileName)
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
        //observer.didEnumerate([FileProviderItem(identifier: NSFileProviderItemIdentifier("a file"))])
        observer.finishEnumerating(upTo: nil)
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

    func completeObserver(observer: NSFileProviderEnumerationObserver, numPage: Int, itemMetadatas: [NextcloudItemMetadataTable]?) {
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
}

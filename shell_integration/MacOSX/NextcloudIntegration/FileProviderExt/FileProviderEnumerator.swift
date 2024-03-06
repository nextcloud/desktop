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
        FileProviderEnumerator.isSystemIdentifier(enumeratedItemIdentifier)
    }

    // TODO: actually use this in NCKit and server requests
    private let anchor = NSFileProviderSyncAnchor(Date().description.data(using: .utf8)!)
    private static let maxItemsPerFileProviderPage = 100
    let ncAccount: NextcloudAccount
    let ncKit: NextcloudKit
    let fastEnumeration: Bool
    var serverUrl: String = ""
    var isInvalidated = false

    private static func isSystemIdentifier(_ identifier: NSFileProviderItemIdentifier) -> Bool {
        identifier == .rootContainer || identifier == .trashContainer || identifier == .workingSet
    }
    
    init(
        enumeratedItemIdentifier: NSFileProviderItemIdentifier,
        ncAccount: NextcloudAccount,
        ncKit: NextcloudKit,
        fastEnumeration: Bool = true
    ) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        self.ncAccount = ncAccount
        self.ncKit = ncKit
        self.fastEnumeration = fastEnumeration

        if FileProviderEnumerator.isSystemIdentifier(enumeratedItemIdentifier) {
            Logger.enumeration.debug(
                "Providing enumerator for a system defined container: \(enumeratedItemIdentifier.rawValue, privacy: .public)"
            )
            serverUrl = ncAccount.davFilesUrl
        } else {
            Logger.enumeration.debug(
                "Providing enumerator for item with identifier: \(enumeratedItemIdentifier.rawValue, privacy: .public)"
            )
            let dbManager = NextcloudFilesDatabaseManager.shared

            enumeratedItemMetadata = dbManager.itemMetadataFromFileProviderItemIdentifier(
                enumeratedItemIdentifier)
            if enumeratedItemMetadata != nil {
                serverUrl =
                    enumeratedItemMetadata!.serverUrl + "/" + enumeratedItemMetadata!.fileName
            } else {
                Logger.enumeration.error(
                    "Could not find itemMetadata for file with identifier: \(enumeratedItemIdentifier.rawValue, privacy: .public)"
                )
            }
        }

        Logger.enumeration.info(
            "Set up enumerator for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
        )
        super.init()
    }

    func invalidate() {
        Logger.enumeration.debug(
            "Enumerator is being invalidated for item with identifier: \(self.enumeratedItemIdentifier.rawValue, privacy: .public)"
        )
        isInvalidated = true
    }

    // MARK: - Protocol methods

    func enumerateItems(
        for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage
    ) {
        Logger.enumeration.debug(
            "Received enumerate items request for enumerator with user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
        )
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

        if enumeratedItemIdentifier == .trashContainer {
            Logger.enumeration.debug(
                "Enumerating trash set for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
            )
            // TODO!

            observer.finishEnumerating(upTo: nil)
            return
        }

        // Handle the working set as if it were the root container
        // If we do a full server scan per the recommendations of the File Provider documentation,
        // we will be stuck for a huge period of time without being able to access files as the
        // entire server gets scanned. Instead, treat the working set as the root container here.
        // Then, when we enumerate changes, we'll go through everything -- while we can still
        // navigate a little bit in Finder, file picker, etc

        guard serverUrl != "" else {
            Logger.enumeration.error(
                "Enumerator has empty serverUrl -- can't enumerate that! For identifier: \(self.enumeratedItemIdentifier.rawValue, privacy: .public)"
            )
            observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
            return
        }

        // TODO: Make better use of pagination and handle paging properly
        if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage
            || page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage
        {
            Logger.enumeration.debug(
                "Enumerating initial page for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
            )

            FileProviderEnumerator.readServerUrl(serverUrl, ncAccount: ncAccount, ncKit: ncKit) {
                metadatas, _, _, _, readError in

                guard readError == nil else {
                    Logger.enumeration.error(
                        "Finishing enumeration for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public) with error \(readError!.localizedDescription, privacy: .public)"
                    )

                    let nkReadError = NKError(error: readError!)
                    observer.finishEnumeratingWithError(nkReadError.fileProviderError)
                    return
                }

                guard let metadatas else {
                    Logger.enumeration.error(
                        "Finishing enumeration for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public) with invalid metadatas."
                    )
                    observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                    return
                }

                Logger.enumeration.info(
                    "Finished reading serverUrl: \(self.serverUrl, privacy: .public) for user: \(self.ncAccount.ncKitAccount, privacy: .public). Processed \(metadatas.count) metadatas"
                )

                FileProviderEnumerator.completeEnumerationObserver(
                    observer, ncKit: self.ncKit, numPage: 1, itemMetadatas: metadatas)
            }

            return
        }

        let numPage = Int(String(data: page.rawValue, encoding: .utf8)!)!
        Logger.enumeration.debug(
            "Enumerating page \(numPage, privacy: .public) for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
        )
        // TODO: Handle paging properly
        // FileProviderEnumerator.completeObserver(observer, ncKit: ncKit, numPage: numPage, itemMetadatas: nil)
        observer.finishEnumerating(upTo: nil)
    }

    func enumerateChanges(
        for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor
    ) {
        Logger.enumeration.debug(
            "Received enumerate changes request for enumerator for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
        )
        /*
         - query the server for updates since the passed-in sync anchor (TODO)

         If this is an enumerator for the working set:
         - note the changes in your local database

         - inform the observer about item deletions and updates (modifications + insertions)
         - inform the observer when you have finished enumerating up to a subsequent sync anchor
         */

        if enumeratedItemIdentifier == .workingSet {
            Logger.enumeration.debug(
                "Enumerating changes in working set for user: \(self.ncAccount.ncKitAccount, privacy: .public)"
            )

            // Unlike when enumerating items we can't progressively enumerate items as we need to wait to resolve which items are truly deleted and which
            // have just been moved elsewhere.
            fullRecursiveScan(
                ncAccount: ncAccount,
                ncKit: ncKit,
                scanChangesOnly: true
            ) { _, newMetadatas, updatedMetadatas, deletedMetadatas, error in

                if self.isInvalidated {
                    Logger.enumeration.info(
                        "Enumerator invalidated during working set change scan. For user: \(self.ncAccount.ncKitAccount, privacy: .public)"
                    )
                    observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                    return
                }

                guard error == nil else {
                    Logger.enumeration.info(
                        "Finished recursive change enumeration of working set for user: \(self.ncAccount.ncKitAccount, privacy: .public) with error: \(error!.errorDescription, privacy: .public)"
                    )
                    observer.finishEnumeratingWithError(error!.fileProviderError)
                    return
                }

                Logger.enumeration.info(
                    "Finished recursive change enumeration of working set for user: \(self.ncAccount.ncKitAccount, privacy: .public). Enumerating items."
                )

                FileProviderEnumerator.completeChangesObserver(
                    observer,
                    anchor: anchor,
                    ncKit: self.ncKit,
                    newMetadatas: newMetadatas,
                    updatedMetadatas: updatedMetadatas,
                    deletedMetadatas: deletedMetadatas)
            }
            return
        } else if enumeratedItemIdentifier == .trashContainer {
            Logger.enumeration.debug(
                "Enumerating changes in trash set for user: \(self.ncAccount.ncKitAccount, privacy: .public)"
            )
            // TODO!

            observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
            return
        }

        Logger.enumeration.info(
            "Enumerating changes for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
        )

        // No matter what happens here we finish enumeration in some way, either from the error
        // handling below or from the completeChangesObserver
        // TODO: Move to the sync engine extension
        FileProviderEnumerator.readServerUrl(
            serverUrl, ncAccount: ncAccount, ncKit: ncKit, stopAtMatchingEtags: true
        ) { _, newMetadatas, updatedMetadatas, deletedMetadatas, readError in

            // If we get a 404 we might add more deleted metadatas
            var currentDeletedMetadatas: [NextcloudItemMetadataTable] = []
            if let notNilDeletedMetadatas = deletedMetadatas {
                currentDeletedMetadatas = notNilDeletedMetadatas
            }

            guard readError == nil else {
                Logger.enumeration.error(
                    "Finishing enumeration of changes for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public) with error: \(readError!.localizedDescription, privacy: .public)"
                )

                let nkReadError = NKError(error: readError!)
                let fpError = nkReadError.fileProviderError

                if nkReadError.isNotFoundError {
                    Logger.enumeration.info(
                        "404 error means item no longer exists. Deleting metadata and reporting \(self.serverUrl, privacy: .public) as deletion without error"
                    )

                    guard let itemMetadata = self.enumeratedItemMetadata else {
                        Logger.enumeration.error(
                            "Invalid enumeratedItemMetadata, could not delete metadata nor report deletion"
                        )
                        observer.finishEnumeratingWithError(fpError)
                        return
                    }

                    let dbManager = NextcloudFilesDatabaseManager.shared
                    if itemMetadata.directory {
                        if let deletedDirectoryMetadatas =
                            dbManager.deleteDirectoryAndSubdirectoriesMetadata(
                                ocId: itemMetadata.ocId)
                        {
                            currentDeletedMetadatas += deletedDirectoryMetadatas
                        } else {
                            Logger.enumeration.error(
                                "Something went wrong when recursively deleting directory not found."
                            )
                        }
                    } else {
                        dbManager.deleteItemMetadata(ocId: itemMetadata.ocId)
                    }

                    FileProviderEnumerator.completeChangesObserver(
                        observer, anchor: anchor, ncKit: self.ncKit, newMetadatas: nil,
                        updatedMetadatas: nil,
                        deletedMetadatas: [itemMetadata])
                    return
                } else if nkReadError.isNoChangesError {  // All is well, just no changed etags
                    Logger.enumeration.info(
                        "Error was to say no changed files -- not bad error. Finishing change enumeration."
                    )
                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
                    return
                }

                observer.finishEnumeratingWithError(fpError)
                return
            }

            Logger.enumeration.info(
                "Finished reading serverUrl: \(self.serverUrl, privacy: .public) for user: \(self.ncAccount.ncKitAccount, privacy: .public)"
            )

            FileProviderEnumerator.completeChangesObserver(
                observer,
                anchor: anchor,
                ncKit: self.ncKit,
                newMetadatas: newMetadatas,
                updatedMetadatas: updatedMetadatas,
                deletedMetadatas: deletedMetadatas)
        }
    }

    func currentSyncAnchor(completionHandler: @escaping (NSFileProviderSyncAnchor?) -> Void) {
        completionHandler(anchor)
    }

    // MARK: - Helper methods

    private static func metadatasToFileProviderItems(
        _ itemMetadatas: [NextcloudItemMetadataTable], ncKit: NextcloudKit,
        completionHandler: @escaping (_ items: [NSFileProviderItem]) -> Void
    ) {
        var items: [NSFileProviderItem] = []

        let conversionQueue = DispatchQueue(
            label: "metadataToItemConversionQueue", qos: .userInitiated, attributes: .concurrent)
        let appendQueue = DispatchQueue(label: "enumeratorItemAppendQueue", qos: .userInitiated)  // Serial queue
        let dispatchGroup = DispatchGroup()

        for itemMetadata in itemMetadatas {
            conversionQueue.async(group: dispatchGroup) {
                if itemMetadata.e2eEncrypted {
                    Logger.enumeration.info(
                        "Skipping encrypted metadata in enumeration: \(itemMetadata.ocId, privacy: .public) \(itemMetadata.fileName, privacy: .public)"
                    )
                    return
                }

                if let parentItemIdentifier = NextcloudFilesDatabaseManager.shared
                    .parentItemIdentifierFromMetadata(itemMetadata)
                {
                    let item = FileProviderItem(
                        metadata: itemMetadata, parentItemIdentifier: parentItemIdentifier,
                        ncKit: ncKit)
                    Logger.enumeration.debug(
                        "Will enumerate item with ocId: \(itemMetadata.ocId, privacy: .public) and name: \(itemMetadata.fileName, privacy: .public)"
                    )

                    appendQueue.async(group: dispatchGroup) {
                        items.append(item)
                    }
                } else {
                    Logger.enumeration.error(
                        "Could not get valid parentItemIdentifier for item with ocId: \(itemMetadata.ocId, privacy: .public) and name: \(itemMetadata.fileName, privacy: .public), skipping enumeration"
                    )
                }
            }
        }

        dispatchGroup.notify(queue: DispatchQueue.main) {
            completionHandler(items)
        }
    }

    private static func fileProviderPageforNumPage(_ numPage: Int) -> NSFileProviderPage {
        NSFileProviderPage("\(numPage)".data(using: .utf8)!)
    }

    private static func completeEnumerationObserver(
        _ observer: NSFileProviderEnumerationObserver, ncKit: NextcloudKit, numPage: Int,
        itemMetadatas: [NextcloudItemMetadataTable]
    ) {
        metadatasToFileProviderItems(itemMetadatas, ncKit: ncKit) { items in
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
            observer.finishEnumerating(upTo: fileProviderPageforNumPage(numPage))
        }
    }

    private static func completeChangesObserver(
        _ observer: NSFileProviderChangeObserver, anchor: NSFileProviderSyncAnchor,
        ncKit: NextcloudKit,
        newMetadatas: [NextcloudItemMetadataTable]?,
        updatedMetadatas: [NextcloudItemMetadataTable]?,
        deletedMetadatas: [NextcloudItemMetadataTable]?
    ) {
        guard newMetadatas != nil || updatedMetadatas != nil || deletedMetadatas != nil else {
            Logger.enumeration.error(
                "Received invalid newMetadatas, updatedMetadatas or deletedMetadatas. Finished enumeration of changes with error."
            )
            observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
            return
        }

        // Observer does not care about new vs updated, so join
        var allUpdatedMetadatas: [NextcloudItemMetadataTable] = []
        var allDeletedMetadatas: [NextcloudItemMetadataTable] = []

        if let newMetadatas {
            allUpdatedMetadatas += newMetadatas
        }

        if let updatedMetadatas {
            allUpdatedMetadatas += updatedMetadatas
        }

        if let deletedMetadatas {
            allDeletedMetadatas = deletedMetadatas
        }

        let allFpItemDeletionsIdentifiers = Array(
            allDeletedMetadatas.map { NSFileProviderItemIdentifier($0.ocId) })
        if !allFpItemDeletionsIdentifiers.isEmpty {
            observer.didDeleteItems(withIdentifiers: allFpItemDeletionsIdentifiers)
        }

        metadatasToFileProviderItems(allUpdatedMetadatas, ncKit: ncKit) { updatedItems in

            if !updatedItems.isEmpty {
                observer.didUpdate(updatedItems)
            }

            Logger.enumeration.info(
                "Processed \(updatedItems.count) new or updated metadatas, \(allDeletedMetadatas.count) deleted metadatas."
            )
            observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
        }
    }
}

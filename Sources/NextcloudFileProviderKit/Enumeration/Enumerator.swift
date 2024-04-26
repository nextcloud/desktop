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

public class Enumerator: NSObject, NSFileProviderEnumerator {
    let enumeratedItemIdentifier: NSFileProviderItemIdentifier
    private var enumeratedItemMetadata: ItemMetadata?
    private var enumeratingSystemIdentifier: Bool {
        Self.isSystemIdentifier(enumeratedItemIdentifier)
    }
    let domain: NSFileProviderDomain?

    // TODO: actually use this in NCKit and server requests
    private let anchor = NSFileProviderSyncAnchor(Date().description.data(using: .utf8)!)
    private static let maxItemsPerFileProviderPage = 100
    static let logger = Logger(subsystem: Logger.subsystem, category: "enumerator")
    let ncAccount: Account
    let remoteInterface: RemoteInterface
    let fastEnumeration: Bool
    var serverUrl: String = ""
    var isInvalidated = false

    private static func isSystemIdentifier(_ identifier: NSFileProviderItemIdentifier) -> Bool {
        identifier == .rootContainer || identifier == .trashContainer || identifier == .workingSet
    }
    
    public init(
        enumeratedItemIdentifier: NSFileProviderItemIdentifier,
        ncAccount: Account,
        remoteInterface: RemoteInterface,
        domain: NSFileProviderDomain? = nil,
        fastEnumeration: Bool = true
    ) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        self.ncAccount = ncAccount
        self.remoteInterface = remoteInterface
        self.domain = domain
        self.fastEnumeration = fastEnumeration

        if Self.isSystemIdentifier(enumeratedItemIdentifier) {
            Self.logger.debug(
                "Providing enumerator for a system defined container: \(enumeratedItemIdentifier.rawValue, privacy: .public)"
            )
            serverUrl = ncAccount.davFilesUrl
        } else {
            Self.logger.debug(
                "Providing enumerator for item with identifier: \(enumeratedItemIdentifier.rawValue, privacy: .public)"
            )
            let dbManager = FilesDatabaseManager.shared

            enumeratedItemMetadata = dbManager.itemMetadataFromFileProviderItemIdentifier(
                enumeratedItemIdentifier)
            if let enumeratedItemMetadata {
                serverUrl =
                    enumeratedItemMetadata.serverUrl + "/" + enumeratedItemMetadata.fileName
            } else {
                Self.logger.error(
                    "Could not find itemMetadata for file with identifier: \(enumeratedItemIdentifier.rawValue, privacy: .public)"
                )
            }
        }

        Self.logger.info(
            "Set up enumerator for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
        )
        super.init()
    }

    public func invalidate() {
        Self.logger.debug(
            "Enumerator is being invalidated for item with identifier: \(self.enumeratedItemIdentifier.rawValue, privacy: .public)"
        )
        isInvalidated = true
    }

    // MARK: - Protocol methods

    public func enumerateItems(
        for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage
    ) {
        Self.logger.debug(
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
            Self.logger.debug(
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
            Self.logger.error(
                "Enumerator has empty serverUrl -- can't enumerate that! For identifier: \(self.enumeratedItemIdentifier.rawValue, privacy: .public)"
            )
            observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
            return
        }

        // TODO: Make better use of pagination and handle paging properly
        if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage
            || page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage
        {
            Self.logger.debug(
                "Enumerating initial page for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
            )

            Task {
                let (metadatas, _, _, _, readError) = await Self.readServerUrl(
                    serverUrl, ncAccount: ncAccount, remoteInterface: remoteInterface
                )

                guard readError == nil else {
                    Self.logger.error(
                        "Finishing enumeration for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public) with error \(readError!.errorDescription, privacy: .public)"
                    )

                    // TODO: Refactor for conciseness
                    let error =
                        readError?.fileProviderError ?? NSFileProviderError(.cannotSynchronize)
                    observer.finishEnumeratingWithError(error)
                    return
                }

                guard let metadatas else {
                    Self.logger.error(
                        "Finishing enumeration for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public) with invalid metadatas."
                    )
                    observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                    return
                }

                Self.logger.info(
                    "Finished reading serverUrl: \(self.serverUrl, privacy: .public) for user: \(self.ncAccount.ncKitAccount, privacy: .public). Processed \(metadatas.count) metadatas"
                )

                Self.completeEnumerationObserver(
                    observer, 
                    remoteInterface: remoteInterface,
                    numPage: 1,
                    itemMetadatas: metadatas
                )
            }

            return
        }

        let numPage = Int(String(data: page.rawValue, encoding: .utf8)!)!
        Self.logger.debug(
            "Enumerating page \(numPage, privacy: .public) for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
        )
        // TODO: Handle paging properly
        // Self.completeObserver(observer, ncKit: ncKit, numPage: numPage, itemMetadatas: nil)
        observer.finishEnumerating(upTo: nil)
    }

    public func enumerateChanges(
        for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor
    ) {
        Self.logger.debug(
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
            Self.logger.debug(
                "Enumerating changes in working set for user: \(self.ncAccount.ncKitAccount, privacy: .public)"
            )

            // Unlike when enumerating items we can't progressively enumerate items as we need to wait to resolve which items are truly deleted and which
            // have just been moved elsewhere.
            Task {
                let (
                    _, newMetadatas, updatedMetadatas, deletedMetadatas, error
                ) = await fullRecursiveScan(
                    ncAccount: ncAccount, remoteInterface: remoteInterface, scanChangesOnly: true
                )

                if self.isInvalidated {
                    Self.logger.info(
                        """
                        Enumerator invalidated during working set change scan.
                        For user: \(self.ncAccount.ncKitAccount, privacy: .public)
                        """
                    )
                    observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                    return
                }

                guard error == nil else {
                    Self.logger.info(
                        """
                        Finished recursive change enumeration of working set for user:
                        \(self.ncAccount.ncKitAccount, privacy: .public)
                        with error: \(error!.errorDescription, privacy: .public)
                        """
                    )
                    // TODO: Refactor for conciseness
                    let fpError =
                        error?.fileProviderError ?? NSFileProviderError(.cannotSynchronize)
                    observer.finishEnumeratingWithError(fpError)
                    return
                }

                Self.logger.info(
                    """
                    Finished recursive change enumeration of working set for user:
                    \(self.ncAccount.ncKitAccount, privacy: .public). Enumerating items.
                    """
                )

                Self.completeChangesObserver(
                    observer,
                    anchor: anchor,
                    remoteInterface: remoteInterface,
                    newMetadatas: newMetadatas,
                    updatedMetadatas: updatedMetadatas,
                    deletedMetadatas: deletedMetadatas
                )
            }
            return
        } else if enumeratedItemIdentifier == .trashContainer {
            Self.logger.debug(
                "Enumerating changes in trash set for user: \(self.ncAccount.ncKitAccount, privacy: .public)"
            )
            // TODO!

            observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
            return
        }

        Self.logger.info(
            "Enumerating changes for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public)"
        )

        // No matter what happens here we finish enumeration in some way, either from the error
        // handling below or from the completeChangesObserver
        // TODO: Move to the sync engine extension
        Task {
            let (
                _, newMetadatas, updatedMetadatas, deletedMetadatas, readError
            ) = await Self.readServerUrl(
                serverUrl,
                ncAccount: ncAccount,
                remoteInterface: remoteInterface,
                stopAtMatchingEtags: true
            )

            // If we get a 404 we might add more deleted metadatas
            var currentDeletedMetadatas: [ItemMetadata] = []
            if let notNilDeletedMetadatas = deletedMetadatas {
                currentDeletedMetadatas = notNilDeletedMetadatas
            }

            guard readError == nil else {
                Self.logger.error(
                    "Finishing enumeration of changes for user: \(self.ncAccount.ncKitAccount, privacy: .public) with serverUrl: \(self.serverUrl, privacy: .public) with error: \(readError!.errorDescription, privacy: .public)"
                )

                let error = readError!.fileProviderError ?? NSFileProviderError(.cannotSynchronize)

                if readError!.isNotFoundError {
                    Self.logger.info(
                        "404 error means item no longer exists. Deleting metadata and reporting \(self.serverUrl, privacy: .public) as deletion without error"
                    )

                    guard let itemMetadata = self.enumeratedItemMetadata else {
                        Self.logger.error(
                            "Invalid enumeratedItemMetadata, could not delete metadata nor report deletion"
                        )
                        observer.finishEnumeratingWithError(error)
                        return
                    }

                    let dbManager = FilesDatabaseManager.shared
                    if itemMetadata.directory {
                        if let deletedDirectoryMetadatas =
                            dbManager.deleteDirectoryAndSubdirectoriesMetadata(
                                ocId: itemMetadata.ocId)
                        {
                            currentDeletedMetadatas += deletedDirectoryMetadatas
                        } else {
                            Self.logger.error(
                                "Something went wrong when recursively deleting directory not found."
                            )
                        }
                    } else {
                        dbManager.deleteItemMetadata(ocId: itemMetadata.ocId)
                    }

                    Self.completeChangesObserver(
                        observer, 
                        anchor: anchor,
                        remoteInterface: remoteInterface,
                        newMetadatas: nil,
                        updatedMetadatas: nil,
                        deletedMetadatas: [itemMetadata]
                    )
                    return
                } else if readError!.isNoChangesError {  // All is well, just no changed etags
                    Self.logger.info(
                        "Error was to say no changed files -- not bad error. Finishing change enumeration."
                    )
                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
                    return
                }

                observer.finishEnumeratingWithError(error)
                return
            }

            Self.logger.info(
                "Finished reading serverUrl: \(self.serverUrl, privacy: .public) for user: \(self.ncAccount.ncKitAccount, privacy: .public)"
            )

            Self.completeChangesObserver(
                observer,
                anchor: anchor,
                remoteInterface: remoteInterface,
                newMetadatas: newMetadatas,
                updatedMetadatas: updatedMetadatas,
                deletedMetadatas: deletedMetadatas
            )
        }
    }

    public func currentSyncAnchor(completionHandler: @escaping (NSFileProviderSyncAnchor?) -> Void) {
        completionHandler(anchor)
    }

    // MARK: - Helper methods

    // TODO: Use async group
    private static func metadatasToFileProviderItems(
        _ itemMetadatas: [ItemMetadata], 
        remoteInterface: RemoteInterface,
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
                    Self.logger.info(
                        "Skipping encrypted metadata in enumeration: \(itemMetadata.ocId, privacy: .public) \(itemMetadata.fileName, privacy: .public)"
                    )
                    return
                }

                let dbManager = FilesDatabaseManager.shared
                if let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(
                    itemMetadata
                ) {
                    let item = Item(
                        metadata: itemMetadata, 
                        parentItemIdentifier: parentItemIdentifier,
                        remoteInterface: remoteInterface
                    )
                    Self.logger.debug(
                        "Will enumerate item with ocId: \(itemMetadata.ocId, privacy: .public) and name: \(itemMetadata.fileName, privacy: .public)"
                    )

                    appendQueue.async(group: dispatchGroup) {
                        items.append(item)
                    }
                } else {
                    Self.logger.error(
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
        _ observer: NSFileProviderEnumerationObserver, 
        remoteInterface: RemoteInterface,
        numPage: Int,
        itemMetadatas: [ItemMetadata]
    ) {
        metadatasToFileProviderItems(itemMetadatas, remoteInterface: remoteInterface) { items in
            observer.didEnumerate(items)
            Self.logger.info("Did enumerate \(items.count) items")

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
        remoteInterface: RemoteInterface,
        newMetadatas: [ItemMetadata]?,
        updatedMetadatas: [ItemMetadata]?,
        deletedMetadatas: [ItemMetadata]?
    ) {
        guard newMetadatas != nil || updatedMetadatas != nil || deletedMetadatas != nil else {
            Self.logger.error(
                "Received invalid newMetadatas, updatedMetadatas or deletedMetadatas. Finished enumeration of changes with error."
            )
            observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
            return
        }

        // Observer does not care about new vs updated, so join
        var allUpdatedMetadatas: [ItemMetadata] = []
        var allDeletedMetadatas: [ItemMetadata] = []

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

        metadatasToFileProviderItems(
            allUpdatedMetadatas, remoteInterface: remoteInterface
        ) { updatedItems in

            if !updatedItems.isEmpty {
                observer.didUpdate(updatedItems)
            }

            Self.logger.info(
                "Processed \(updatedItems.count) new or updated metadatas, \(allDeletedMetadatas.count) deleted metadatas."
            )
            observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
        }
    }
}

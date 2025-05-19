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
    private var enumeratedItemMetadata: SendableItemMetadata?
    private var enumeratingSystemIdentifier: Bool {
        Self.isSystemIdentifier(enumeratedItemIdentifier)
    }
    let domain: NSFileProviderDomain?
    let dbManager: FilesDatabaseManager

    // TODO: actually use this in NCKit and server requests
    private let anchor = NSFileProviderSyncAnchor(Date().description.data(using: .utf8)!)
    private static let maxItemsPerFileProviderPage = 100
    static let logger = Logger(subsystem: Logger.subsystem, category: "enumerator")
    let account: Account
    let remoteInterface: RemoteInterface
    let fastEnumeration: Bool
    var serverUrl: String = ""
    var isInvalidated = false
    weak var listener: EnumerationListener?

    private static func isSystemIdentifier(_ identifier: NSFileProviderItemIdentifier) -> Bool {
        identifier == .rootContainer || identifier == .trashContainer || identifier == .workingSet
    }

    public init(
        enumeratedItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        domain: NSFileProviderDomain? = nil,
        fastEnumeration: Bool = true,
        listener: EnumerationListener? = nil
    ) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        self.remoteInterface = remoteInterface
        self.account = account
        self.dbManager = dbManager
        self.domain = domain
        self.fastEnumeration = fastEnumeration
        self.listener = listener

        if Self.isSystemIdentifier(enumeratedItemIdentifier) {
            Self.logger.debug(
                """
                Providing enumerator for a system defined container:
                \(enumeratedItemIdentifier.rawValue, privacy: .public)
                """
            )
            serverUrl = account.davFilesUrl
        } else {
            Self.logger.debug(
                """
                Providing enumerator for item with identifier:
                \(enumeratedItemIdentifier.rawValue, privacy: .public)
                """
            )

            enumeratedItemMetadata = dbManager.itemMetadataFromFileProviderItemIdentifier(
                enumeratedItemIdentifier)
            if let enumeratedItemMetadata {
                serverUrl =
                    enumeratedItemMetadata.serverUrl + "/" + enumeratedItemMetadata.fileName
            } else {
                Self.logger.error(
                    """
                    Could not find itemMetadata for file with identifier:
                    \(enumeratedItemIdentifier.rawValue, privacy: .public)
                    """
                )
            }
        }

        Self.logger.info(
            """
            Set up enumerator for user: \(self.account.ncKitAccount, privacy: .public)
            with serverUrl: \(self.serverUrl, privacy: .public)
            """
        )
        super.init()
    }

    public func invalidate() {
        Self.logger.debug(
            """
            Enumerator is being invalidated for item with identifier:
            \(self.enumeratedItemIdentifier.rawValue, privacy: .public)
            """
        )
        isInvalidated = true
    }

    // MARK: - Protocol methods

    public func enumerateItems(
        for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage
    ) {
        let actionId = UUID()
        listener?.enumerationActionStarted(actionId: actionId)

        Self.logger.debug(
            """
            Received enumerate items request for enumerator with user:
            \(self.account.ncKitAccount, privacy: .public)
            with serverUrl: \(self.serverUrl, privacy: .public)
            """
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
                """
                Enumerating trash set for user: \(self.account.ncKitAccount, privacy: .public)
                with serverUrl: \(self.serverUrl, privacy: .public)
                """
            )

            Task {
                let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(
                    account: account, options: .init(), taskHandler: { _ in }
                )
                guard let capabilities, error == .success else {
                    Self.logger.error(
                        """
                        Could not acquire capabilities, cannot check trash.
                            Error: \(error.errorDescription, privacy: .public)
                        """)
                    observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                    return
                }
                guard capabilities.files?.undelete == true else {
                    Self.logger.error("Trash is unsupported on server, cannot enumerate items.")
                    observer.finishEnumeratingWithError(
                        NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError)
                    )
                    return
                }

                let (_, trashedItems, _, trashReadError) = await remoteInterface.trashedItems(
                    account: account,
                    options: .init(),
                    taskHandler: { task in
                        if let domain = self.domain {
                            NSFileProviderManager(for: domain)?.register(
                                task,
                                forItemWithIdentifier: self.enumeratedItemIdentifier,
                                completionHandler: { _ in }
                            )
                        }
                    }
                )

                guard trashReadError == .success else {
                    let error = trashReadError.fileProviderError(
                        handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier
                    ) ?? NSFileProviderError(.cannotSynchronize)
                    listener?.enumerationActionFailed(actionId: actionId, error: error)
                    observer.finishEnumeratingWithError(error)
                    return
                }

                Self.completeEnumerationObserver(
                    observer,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    numPage: 1,
                    trashItems: trashedItems
                )
                listener?.enumerationActionFinished(actionId: actionId)
            }
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
                """
                Enumerator has empty serverUrl -- can't enumerate that!
                For identifier: \(self.enumeratedItemIdentifier.rawValue, privacy: .public)
                """
            )
            let error = NSError.fileProviderErrorForNonExistentItem(
                withIdentifier: self.enumeratedItemIdentifier
            )
            listener?.enumerationActionFailed(actionId: actionId, error: error)
            observer.finishEnumeratingWithError(error)
            return
        }

        // TODO: Make better use of pagination and handle paging properly
        if page == NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage
            || page == NSFileProviderPage.initialPageSortedByName as NSFileProviderPage
        {
            Self.logger.debug(
                """
                Enumerating initial page for user: \(self.account.ncKitAccount, privacy: .public)
                with serverUrl: \(self.serverUrl, privacy: .public)
                """
            )

            Task {
                let (metadatas, _, _, _, readError) = await Self.readServerUrl(
                    serverUrl,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager
                )

                guard readError == nil else {
                    Self.logger.error(
                        """
                        "Finishing enumeration for: \(self.account.ncKitAccount, privacy: .public)
                        with serverUrl: \(self.serverUrl, privacy: .public) 
                        with error \(readError!.errorDescription, privacy: .public)
                        """
                    )

                    // TODO: Refactor for conciseness
                    let error = readError?.fileProviderError(
                        handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier
                    ) ?? NSFileProviderError(.cannotSynchronize)
                    listener?.enumerationActionFailed(actionId: actionId, error: error)
                    observer.finishEnumeratingWithError(error)
                    return
                }

                guard let metadatas else {
                    Self.logger.error(
                        """
                        Finishing enumeration for: \(self.account.ncKitAccount, privacy: .public)
                        with serverUrl: \(self.serverUrl, privacy: .public)
                        with invalid metadatas.
                        """
                    )
                    listener?.enumerationActionFailed(
                        actionId: actionId, error: NSFileProviderError(.cannotSynchronize)
                    )
                    observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                    return
                }

                Self.logger.info(
                    """
                    Finished reading serverUrl: \(self.serverUrl, privacy: .public)
                    for user: \(self.account.ncKitAccount, privacy: .public).
                    Processed \(metadatas.count) metadatas
                    """
                )

                Self.completeEnumerationObserver(
                    observer,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    numPage: 1,
                    itemMetadatas: metadatas
                )
                listener?.enumerationActionFinished(actionId: actionId)
            }

            return
        }

        let numPage = Int(String(data: page.rawValue, encoding: .utf8)!)!
        Self.logger.debug(
            """
            Enumerating page \(numPage, privacy: .public)
            for user: \(self.account.ncKitAccount, privacy: .public)
            with serverUrl: \(self.serverUrl, privacy: .public)
            """
        )
        // TODO: Handle paging properly
        // Self.completeObserver(observer, ncKit: ncKit, numPage: numPage, itemMetadatas: nil)
        listener?.enumerationActionFinished(actionId: actionId)
        observer.finishEnumerating(upTo: nil)
    }

    public func enumerateChanges(
        for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor
    ) {
        let actionId = UUID()
        listener?.enumerationActionStarted(actionId: actionId)

        Self.logger.debug(
            """
            Received enumerate changes request for enumerator for user:
            \(self.account.ncKitAccount, privacy: .public)
            with serverUrl: \(self.serverUrl, privacy: .public)
            """
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
                "Enumerating changes in working set for: \(self.account.ncKitAccount, privacy: .public)"
            )

            // Unlike when enumerating items we can't progressively enumerate items as we need to 
            // wait to see which items are truly deleted and which have just been moved elsewhere.
            Task {
                let (
                    _, newMetadatas, updatedMetadatas, deletedMetadatas, error
                ) = await fullRecursiveScan(
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    scanChangesOnly: true
                )

                if self.isInvalidated {
                    Self.logger.info(
                        """
                        Enumerator invalidated during working set change scan.
                        For user: \(self.account.ncKitAccount, privacy: .public)
                        """
                    )
                    listener?.enumerationActionFailed(
                        actionId: actionId, error: NSFileProviderError(.cannotSynchronize)
                    )
                    observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                    return
                }

                guard error == nil else {
                    Self.logger.info(
                        """
                        Finished recursive change enumeration of working set for user:
                        \(self.account.ncKitAccount, privacy: .public)
                        with error: \(error!.errorDescription, privacy: .public)
                        """
                    )
                    // TODO: Refactor for conciseness
                    let fpError = error?.fileProviderError(
                        handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier
                    ) ?? NSFileProviderError(.cannotSynchronize)
                    listener?.enumerationActionFailed(actionId: actionId, error: fpError)
                    observer.finishEnumeratingWithError(fpError)
                    return
                }

                Self.logger.info(
                    """
                    Finished recursive change enumeration of working set for user:
                    \(self.account.ncKitAccount, privacy: .public). Enumerating items.
                    """
                )

                Self.completeChangesObserver(
                    observer,
                    anchor: anchor,
                    enumeratedItemIdentifier: self.enumeratedItemIdentifier,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    newMetadatas: newMetadatas,
                    updatedMetadatas: updatedMetadatas,
                    deletedMetadatas: deletedMetadatas
                )
                listener?.enumerationActionFinished(actionId: actionId)
            }
            return
        } else if enumeratedItemIdentifier == .trashContainer {
            Self.logger.debug(
                "Enumerating changes in trash set for: \(self.account.ncKitAccount, privacy: .public)"
            )

            Task {
                let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(
                    account: account, options: .init(), taskHandler: { _ in }
                )
                guard let capabilities, error == .success else {
                    Self.logger.error(
                        """
                        Could not acquire capabilities, cannot check trash.
                            Error: \(error.errorDescription, privacy: .public)
                        """)
                    observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                    return
                }
                guard capabilities.files?.undelete == true else {
                    Self.logger.error("Trash is unsupported on server, cannot enumerate changes.")
                    observer.finishEnumeratingWithError(
                        NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError)
                    )
                    return
                }

                let (_, trashedItems, _, trashReadError) = await remoteInterface.trashedItems(
                    account: account,
                    options: .init(),
                    taskHandler: { task in
                        if let domain = self.domain {
                            NSFileProviderManager(for: domain)?.register(
                                task,
                                forItemWithIdentifier: self.enumeratedItemIdentifier,
                                completionHandler: { _ in }
                            )
                        }
                    }
                )

                guard trashReadError == .success else {
                    let error = trashReadError.fileProviderError(
                        handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier
                    ) ?? NSFileProviderError(.cannotSynchronize)
                    listener?.enumerationActionFailed(actionId: actionId, error: error)
                    observer.finishEnumeratingWithError(error)
                    return
                }

                Self.completeChangesObserver(
                    observer,
                    anchor: anchor,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    trashItems: trashedItems
                )
                listener?.enumerationActionFinished(actionId: actionId)
            }
            return
        }

        Self.logger.info(
            """
            Enumerating changes for user: \(self.account.ncKitAccount, privacy: .public)
            with serverUrl: \(self.serverUrl, privacy: .public)
            """
        )

        // No matter what happens here we finish enumeration in some way, either from the error
        // handling below or from the completeChangesObserver
        // TODO: Move to the sync engine extension
        Task {
            let (
                _, newMetadatas, updatedMetadatas, deletedMetadatas, readError
            ) = await Self.readServerUrl(
                serverUrl,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                stopAtMatchingEtags: true
            )

            // If we get a 404 we might add more deleted metadatas
            var currentDeletedMetadatas: [SendableItemMetadata] = []
            if let notNilDeletedMetadatas = deletedMetadatas {
                currentDeletedMetadatas = notNilDeletedMetadatas
            }

            guard readError == nil else {
                Self.logger.error(
                    """
                    Finishing enumeration of changes for: \(self.account.ncKitAccount, privacy: .public)
                    with serverUrl: \(self.serverUrl, privacy: .public)
                    with error: \(readError!.errorDescription, privacy: .public)
                    """
                )

                let error = readError?.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier
                ) ?? NSFileProviderError(.cannotSynchronize)

                if readError!.isNotFoundError {
                    Self.logger.info(
                        """
                        404 error means item no longer exists.
                        Deleting metadata and reporting \(self.serverUrl, privacy: .public)
                        as deletion without error
                        """
                    )

                    guard let itemMetadata = self.enumeratedItemMetadata else {
                        Self.logger.error(
                            """
                            Invalid enumeratedItemMetadata.
                            Could not delete metadata nor report deletion.
                            """
                        )
                        listener?.enumerationActionFailed(actionId: actionId, error: error)
                        observer.finishEnumeratingWithError(error)
                        return
                    }

                    if itemMetadata.directory {
                        if let deletedDirectoryMetadatas =
                            dbManager.deleteDirectoryAndSubdirectoriesMetadata(
                                ocId: itemMetadata.ocId)
                        {
                            currentDeletedMetadatas += deletedDirectoryMetadatas
                        } else {
                            Self.logger.error(
                                """
                                Something went wrong when recursively deleting directory.
                                It's metadata was not found. Cannot report it as deleted.
                                """
                            )
                        }
                    } else {
                        dbManager.deleteItemMetadata(ocId: itemMetadata.ocId)
                    }

                    Self.completeChangesObserver(
                        observer,
                        anchor: anchor,
                        enumeratedItemIdentifier: self.enumeratedItemIdentifier,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager,
                        newMetadatas: nil,
                        updatedMetadatas: nil,
                        deletedMetadatas: [itemMetadata]
                    )
                    listener?.enumerationActionFinished(actionId: actionId)
                    return
                } else if readError!.isNoChangesError {  // All is well, just no changed etags
                    Self.logger.info(
                        """
                        Error was to say no changed files -- not bad error.
                        Finishing change enumeration.
                        """
                    )
                    listener?.enumerationActionFinished(actionId: actionId)
                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
                    return
                }

                listener?.enumerationActionFailed(actionId: actionId, error: error)
                observer.finishEnumeratingWithError(error)
                return
            }

            Self.logger.info(
                """
                Finished reading serverUrl: \(self.serverUrl, privacy: .public)
                for user: \(self.account.ncKitAccount, privacy: .public)
                """
            )

            Self.completeChangesObserver(
                observer,
                anchor: anchor,
                enumeratedItemIdentifier: self.enumeratedItemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                newMetadatas: newMetadatas,
                updatedMetadatas: updatedMetadatas,
                deletedMetadatas: deletedMetadatas
            )
            listener?.enumerationActionFinished(actionId: actionId)
        }
    }

    public func currentSyncAnchor(completionHandler: @escaping (NSFileProviderSyncAnchor?) -> Void) {
        completionHandler(anchor)
    }

    // MARK: - Helper methods
    static func fileProviderPageforNumPage(_ numPage: Int) -> NSFileProviderPage? {
        return nil
        // TODO: Handle paging properly
        // NSFileProviderPage("\(numPage)".data(using: .utf8)!)
    }

    private static func completeEnumerationObserver(
        _ observer: NSFileProviderEnumerationObserver,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        numPage: Int,
        itemMetadatas: [SendableItemMetadata]
    ) {
        Task {
            let items = await itemMetadatas.toFileProviderItems(
                account: account, remoteInterface: remoteInterface, dbManager: dbManager
            )

            Task { @MainActor in
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
    }

    private static func completeChangesObserver(
        _ observer: NSFileProviderChangeObserver,
        anchor: NSFileProviderSyncAnchor,
        enumeratedItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        newMetadatas: [SendableItemMetadata]?,
        updatedMetadatas: [SendableItemMetadata]?,
        deletedMetadatas: [SendableItemMetadata]?
    ) {
        guard newMetadatas != nil || updatedMetadatas != nil || deletedMetadatas != nil else {
            Self.logger.error(
                """
                Received invalid newMetadatas, updatedMetadatas or deletedMetadatas.
                    Finished enumeration of changes with error.
                """
            )
            observer.finishEnumeratingWithError(
                NSError.fileProviderErrorForNonExistentItem(
                    withIdentifier: enumeratedItemIdentifier
                )
            )
            return
        }

        // Observer does not care about new vs updated, so join
        var allUpdatedMetadatas: [SendableItemMetadata] = []
        var allDeletedMetadatas: [SendableItemMetadata] = []

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

        Task { [allUpdatedMetadatas, allDeletedMetadatas] in
            let updatedItems = await allUpdatedMetadatas.toFileProviderItems(
                account: account, remoteInterface: remoteInterface, dbManager: dbManager
            )

            Task { @MainActor in
                if !updatedItems.isEmpty {
                    observer.didUpdate(updatedItems)
                }

                Self.logger.info(
                    """
                    Processed \(updatedItems.count) new or updated metadatas.
                    \(allDeletedMetadatas.count) deleted metadatas.
                    """
                )
                observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
            }
        }
    }
}

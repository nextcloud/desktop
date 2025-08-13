//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudKit

public class Enumerator: NSObject, NSFileProviderEnumerator {
    let enumeratedItemIdentifier: NSFileProviderItemIdentifier
    private var enumeratedItemMetadata: SendableItemMetadata?
    private var enumeratingSystemIdentifier: Bool {
        Self.isSystemIdentifier(enumeratedItemIdentifier)
    }
    let domain: NSFileProviderDomain?
    let dbManager: FilesDatabaseManager

    private let currentAnchor =
        NSFileProviderSyncAnchor(ISO8601DateFormatter().string(from: Date()).data(using: .utf8)!)
    private let pageItemCount: Int
    let logger: FileProviderLogger
    let account: Account
    let remoteInterface: RemoteInterface
    var isInvalidated = false
    private(set) var serverUrl: String = ""

    private static func isSystemIdentifier(_ identifier: NSFileProviderItemIdentifier) -> Bool {
        identifier == .rootContainer || identifier == .trashContainer || identifier == .workingSet
    }

    public init(
        enumeratedItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        domain: NSFileProviderDomain? = nil,
        pageSize: Int = 100,
        log: any FileProviderLogging
    ) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        self.remoteInterface = remoteInterface
        self.account = account
        self.dbManager = dbManager
        self.domain = domain
        self.pageItemCount = pageSize
        self.logger = FileProviderLogger(category: "Enumerator", log: log)

        if Self.isSystemIdentifier(enumeratedItemIdentifier) {
            logger.debug(
                """
                Providing enumerator for a system defined container:
                \(enumeratedItemIdentifier.rawValue)
                """
            )
            serverUrl = account.davFilesUrl
        } else {
            logger.debug(
                """
                Providing enumerator for item with identifier:
                \(enumeratedItemIdentifier.rawValue)
                """
            )

            enumeratedItemMetadata = dbManager.itemMetadataFromFileProviderItemIdentifier(
                enumeratedItemIdentifier)
            if let enumeratedItemMetadata {
                serverUrl =
                    enumeratedItemMetadata.serverUrl + "/" + enumeratedItemMetadata.fileName
            } else {
                logger.error(
                    """
                    Could not find itemMetadata for file with identifier:
                    \(enumeratedItemIdentifier.rawValue)
                    """
                )
            }
        }

        logger.info(
            """
            Set up enumerator for user: \(self.account.ncKitAccount)
            with serverUrl: \(self.serverUrl)
            """
        )
        super.init()
    }

    public func invalidate() {
        logger.debug(
            """
            Enumerator is being invalidated for item with identifier:
            \(self.enumeratedItemIdentifier.rawValue)
            """
        )
        isInvalidated = true
    }

    // MARK: - Protocol methods

    public func enumerateItems(
        for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage
    ) {
        logger.debug(
            """
            Received enumerate items request for enumerator with user:
            \(self.account.ncKitAccount)
            with serverUrl: \(self.serverUrl)
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
            logger.debug(
                """
                Enumerating trash set for user: \(self.account.ncKitAccount)
                with serverUrl: \(self.serverUrl)
                """
            )

            Task {
                let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(
                    account: account, options: .init(), taskHandler: { _ in }
                )
                guard let capabilities, error == .success else {
                    logger.error(
                        """
                        Could not acquire capabilities, cannot check trash.
                            Error: \(error.errorDescription)
                        """)
                    observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                    return
                }
                guard capabilities.files?.undelete == true else {
                    logger.error("Trash is unsupported on server, cannot enumerate items.")
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
                    observer.finishEnumeratingWithError(error)
                    return
                }

                Self.completeEnumerationObserver(
                    observer,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    numPage: 1,
                    trashItems: trashedItems,
                    log: logger.log
                )
            }
            return
        }

        if enumeratedItemIdentifier == .workingSet {
            logger.info("Upcoming enumeration is of working set.")
            let ncKitAccount = account.ncKitAccount
            // Visited folders and downloaded files
            let materialisedItems = dbManager.materialisedItemMetadatas(account: ncKitAccount)
            completeEnumerationObserver(observer, nextPage: nil, itemMetadatas: materialisedItems)
            return
        }

        guard serverUrl != "" else {
            logger.error(
                """
                Enumerator has empty serverUrl -- can't enumerate that!
                For identifier: \(self.enumeratedItemIdentifier.rawValue)
                """
            )
            let error = NSError.fileProviderErrorForNonExistentItem(
                withIdentifier: self.enumeratedItemIdentifier
            )
            observer.finishEnumeratingWithError(error)
            return
        }

        logger.debug(
            """
            Enumerating page: \(String(data: page.rawValue, encoding: .utf8) ?? "")
                for user: \(self.account.ncKitAccount)
                with serverUrl: \(self.serverUrl)
            """
        )

        Task {
            var providedPage: NSFileProviderPage? = nil // Used for pagination token sent to server
            // Do not pass in the NSFileProviderPage default pages, these are not valid Nextcloud
            // pagination tokens
            var pageIndex = 0
            var pageTotal: Int? = nil
            if page != NSFileProviderPage.initialPageSortedByName as NSFileProviderPage &&
               page != NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage
            {
                if let enumPageResponse =
                    try? JSONDecoder().decode(EnumeratorPageResponse.self, from: page.rawValue)
                {
                    if let token = enumPageResponse.token?.data(using: .utf8) {
                        providedPage = NSFileProviderPage(token)
                    }
                    if let total = enumPageResponse.total {
                        pageTotal = total
                    }
                    pageIndex = enumPageResponse.index
                } else {
                    logger.error("Could not parse page")
                }
            }

            let readResult = await Self.readServerUrl(
                serverUrl,
                pageSettings: nil,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                depth: .targetAndDirectChildren,
                log: logger.log
            )
            let metadatas = readResult.metadatas
            let readError = readResult.readError
            var nextPage = readResult.nextPage

            guard readError == nil else {
                logger.error(
                    """
                    Finishing enumeration for page:
                        \(String(data: page.rawValue, encoding: .utf8) ?? "")
                        for: \(self.account.ncKitAccount)
                        with serverUrl: \(self.serverUrl) 
                        with error \(readError!.errorDescription)
                    """
                )

                // TODO: Refactor for conciseness
                let error = readError?.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier
                ) ?? NSFileProviderError(.cannotSynchronize)
                observer.finishEnumeratingWithError(error)
                return
            }

            guard let metadatas else {
                logger.error(
                    """
                    Finishing enumeration for: \(self.account.ncKitAccount)
                        with serverUrl: \(self.serverUrl)
                        with invalid metadatas.
                    """
                )
                observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                return
            }

            pageTotal = nextPage?.total ?? pageTotal
            if let rPage = nextPage, let pageTotal, rPage.index * pageItemCount >= pageTotal {
                // Server will sometimes provide a valid next page data even though there are no
                // items to enumerate anymore
                logger.debug("No more items to enumerate, stopping paged enumeration")
                nextPage = nil
            }

            nextPage = nil

            logger.info(
                """
                Finished reading page:
                    \(String(data: page.rawValue, encoding: .utf8) ?? "")
                    serverUrl: \(self.serverUrl)
                    for user: \(self.account.ncKitAccount).
                    Processed \(metadatas.count) metadatas
                """
            )

            completeEnumerationObserver(observer, nextPage: nextPage, itemMetadatas: metadatas)
        }
    }

    public func enumerateChanges(
        for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor
    ) {
        logger.debug(
            """
            Received enumerate changes request for enumerator
                for user: \(self.account.ncKitAccount)
                with serverUrl: \(self.serverUrl)
                with sync anchor: \(String(data: anchor.rawValue, encoding: .utf8) ?? "")
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
            logger.debug(
                "Enumerating working set changes for \(self.account.ncKitAccount)"
            )

            let formatter = ISO8601DateFormatter()
            guard let anchorDateString = String(data: anchor.rawValue, encoding: .utf8),
                  let date = formatter.date(from: anchorDateString)
            else {
                logger.error("Couldn't parse sync anchor \(anchor.rawValue)")
                observer.finishEnumeratingWithError(NSFileProviderError(.syncAnchorExpired))
                return
            }
            let pendingChanges = dbManager.pendingWorkingSetChanges(account: account, since: date)

            completeChangesObserver(
                observer,
                anchor: currentAnchor,
                enumeratedItemIdentifier: enumeratedItemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                newMetadatas: [],
                updatedMetadatas: pendingChanges.updated,
                deletedMetadatas: pendingChanges.deleted
            )

            return
        } else if enumeratedItemIdentifier == .trashContainer {
            logger.debug(
                "Enumerating changes in trash set for: \(self.account.ncKitAccount)"
            )

            Task {
                let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(
                    account: account, options: .init(), taskHandler: { _ in }
                )
                guard let capabilities, error == .success else {
                    logger.error(
                        """
                        Could not acquire capabilities, cannot check trash.
                            Error: \(error.errorDescription)
                        """)
                    observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                    return
                }
                guard capabilities.files?.undelete == true else {
                    logger.error("Trash is unsupported on server, cannot enumerate changes.")
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
                    observer.finishEnumeratingWithError(error)
                    return
                }

                await Self.completeChangesObserver(
                    observer,
                    anchor: anchor,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    trashItems: trashedItems,
                    log: logger.log
                )
            }
            return
        }

        logger.info(
            """
            Enumerating changes for user: \(self.account.ncKitAccount)
            with serverUrl: \(self.serverUrl)
            """
        )

        // No matter what happens here we finish enumeration in some way, either from the error
        // handling below or from the completeChangesObserver
        // TODO: Move to the sync engine extension
        Task {
            let (
                _, newMetadatas, updatedMetadatas, deletedMetadatas, _, readError
            ) = await Self.readServerUrl(
                serverUrl,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                log: logger.log
            )

            // If we get a 404 we might add more deleted metadatas
            var currentDeletedMetadatas: [SendableItemMetadata] = []
            if let notNilDeletedMetadatas = deletedMetadatas {
                currentDeletedMetadatas = notNilDeletedMetadatas
            }

            guard readError == nil else {
                logger.error(
                    """
                    Finishing enumeration of changes for: \(self.account.ncKitAccount)
                    with serverUrl: \(self.serverUrl)
                    with error: \(readError!.errorDescription)
                    """
                )

                let error = readError?.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier
                ) ?? NSFileProviderError(.cannotSynchronize)

                if readError!.isNotFoundError {
                    logger.info(
                        """
                        404 error means item no longer exists.
                        Deleting metadata and reporting \(self.serverUrl)
                        as deletion without error
                        """
                    )

                    guard let itemMetadata = self.enumeratedItemMetadata else {
                        logger.error(
                            """
                            Invalid enumeratedItemMetadata.
                            Could not delete metadata nor report deletion.
                            """
                        )
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
                            logger.error(
                                """
                                Something went wrong when recursively deleting directory.
                                It's metadata was not found. Cannot report it as deleted.
                                """
                            )
                        }
                    } else {
                        dbManager.deleteItemMetadata(ocId: itemMetadata.ocId)
                    }

                    completeChangesObserver(
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
                    return
                } else if readError!.isNoChangesError {  // All is well, just no changed etags
                    logger.info(
                        """
                        Error was to say no changed files -- not bad error.
                        Finishing change enumeration.
                        """
                    )
                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
                    return
                }

                observer.finishEnumeratingWithError(error)
                return
            }

            logger.info(
                """
                Finished reading serverUrl: \(self.serverUrl)
                for user: \(self.account.ncKitAccount)
                """
            )

            completeChangesObserver(
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
        }
    }

    public func currentSyncAnchor(completionHandler: @escaping (NSFileProviderSyncAnchor?) -> Void) {
        completionHandler(currentAnchor)
    }

    // MARK: - Helper methods
    static func fileProviderPageforNumPage(_ numPage: Int) -> NSFileProviderPage? {
        return nil
        // TODO: Handle paging properly
        // NSFileProviderPage("\(numPage)".data(using: .utf8)!)
    }

    private func completeEnumerationObserver(
        _ observer: NSFileProviderEnumerationObserver,
        nextPage: EnumeratorPageResponse?,
        itemMetadatas: [SendableItemMetadata],
        handleInvalidParent: Bool = true
    ) {
        Task {
            do {
                let items = try await itemMetadatas.toFileProviderItems(
                    account: account, remoteInterface: remoteInterface, dbManager: dbManager, log: self.logger.log
                )

                Task { @MainActor in
                    observer.didEnumerate(items)
                    logger.info("Did enumerate \(items.count) items")
                    logger.info("Next page is nil: \(nextPage == nil)")

                    if let nextPage, let nextPageData = try? JSONEncoder().encode(nextPage) {
                        logger.info(
                            "Next page: \(String(data: nextPageData, encoding: .utf8) ?? "?")"
                        )
                        observer.finishEnumerating(upTo: NSFileProviderPage(nextPageData))
                    } else {
                        observer.finishEnumerating(upTo: nil)
                    }
                }
            } catch let error as NSError { // This error can only mean a missing parent item identifier
                guard handleInvalidParent else {
                    logger.info("Not handling invalid parent in enumeration")
                    observer.finishEnumeratingWithError(error)
                    return
                }
                do {
                    let metadata = try await attemptInvalidParentRecovery(
                        error: error,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager
                    )

                    completeEnumerationObserver(
                        observer,
                        nextPage: nextPage,
                        itemMetadatas: [metadata] + itemMetadatas,
                        handleInvalidParent: false
                    )
                } catch let error {
                    observer.finishEnumeratingWithError(error)
                }
            }
        }
    }

    private func completeChangesObserver(
        _ observer: NSFileProviderChangeObserver,
        anchor: NSFileProviderSyncAnchor,
        enumeratedItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        newMetadatas: [SendableItemMetadata]?,
        updatedMetadatas: [SendableItemMetadata]?,
        deletedMetadatas: [SendableItemMetadata]?,
        handleInvalidParent: Bool = true
    ) {
        guard newMetadatas != nil || updatedMetadatas != nil || deletedMetadatas != nil else {
            logger.error(
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
            do {
                let updatedItems = try await allUpdatedMetadatas.toFileProviderItems(
                    account: account, remoteInterface: remoteInterface, dbManager: dbManager, log: self.logger.log
                )

                Task { @MainActor in
                    if !updatedItems.isEmpty {
                        observer.didUpdate(updatedItems)
                    }

                    logger.info(
                    """
                    Processed \(updatedItems.count) new or updated metadatas.
                        \(allDeletedMetadatas.count) deleted metadatas.
                    """
                    )
                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
                }
            } catch let error as NSError { // This error can only mean a missing parent item identifier
                guard handleInvalidParent else {
                    logger.info("Not handling invalid parent in change enumeration.")
                    observer.finishEnumeratingWithError(error)
                    return
                }

                logger.info("Attempting handling invalid parent in change enumeration.")

                do {
                    let metadata = try await attemptInvalidParentRecovery(
                        error: error,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager
                    )
                    var modifiedNewMetadatas = newMetadatas
                    modifiedNewMetadatas?.append(metadata)
                    completeChangesObserver(
                        observer,
                        anchor: anchor,
                        enumeratedItemIdentifier: enumeratedItemIdentifier,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager,
                        newMetadatas: modifiedNewMetadatas,
                        updatedMetadatas: updatedMetadatas,
                        deletedMetadatas: deletedMetadatas,
                        handleInvalidParent: false
                    )
                } catch let error {
                    observer.finishEnumeratingWithError(error)
                }
            }
        }
    }

    private func attemptInvalidParentRecovery(
        error: NSError,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager
    ) async throws -> SendableItemMetadata {
        logger.info("Attempting recovery from invalid parent identifier.")
        // Try to recover from errors involving missing metadata for a parent
        let userInfoKey =
            FilesDatabaseManager.ErrorUserInfoKey.missingParentServerUrlAndFileName.rawValue
        guard let urlToEnumerate = (error as NSError).userInfo[userInfoKey] as? String else {
            logger.fault("No missing parent server url and filename in error user info.")
            assert(false)
            throw NSError()
        }

        logger.info(
            "Recovering from invalid parent identifier at \(urlToEnumerate)"
        )
        let (metadatas, _, _, _, _, error) = await Enumerator.readServerUrl(
            urlToEnumerate,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            depth: .target,
            log: logger.log
        )
        guard error == nil || error == .success, let metadata = metadatas?.first else {
            logger.error(
                """
                Problem retrieving parent for metadata.
                    Error: \(error?.errorDescription ?? "NONE")
                    Metadatas: \(metadatas?.count ?? -1)
                """
            )
            throw error?.fileProviderError ?? NSFileProviderError(.cannotSynchronize)
        }
        // Provide it to the caller method so it can ingest it into the database and fix future errs
        return metadata
    }
}

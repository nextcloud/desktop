//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

///
/// The `NSFileProviderEnumerator` implementation to enumerate file provider items and related change sets.
///
public final class Enumerator: NSObject, NSFileProviderEnumerator, Sendable {
    let enumeratedItemIdentifier: NSFileProviderItemIdentifier
    private let enumeratedItemMetadata: SendableItemMetadata?

    private var enumeratingSystemIdentifier: Bool {
        Self.isSystemIdentifier(enumeratedItemIdentifier)
    }

    let domain: NSFileProviderDomain?
    let dbManager: FilesDatabaseManager

    private let currentAnchor = NSFileProviderSyncAnchor(ISO8601DateFormatter().string(from: Date()).data(using: .utf8)!)
    private let pageItemCount: Int
    let logger: FileProviderLogger
    let account: Account
    let remoteInterface: RemoteInterface
    let serverUrl: String

    private static func isSystemIdentifier(_ identifier: NSFileProviderItemIdentifier) -> Bool {
        identifier == .rootContainer || identifier == .trashContainer || identifier == .workingSet
    }

    public init(
        enumeratedItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        domain: NSFileProviderDomain? = nil,
        pageSize: Int = 1000,
        log: any FileProviderLogging
    ) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        self.remoteInterface = remoteInterface
        self.account = account
        self.dbManager = dbManager
        self.domain = domain
        pageItemCount = pageSize
        logger = FileProviderLogger(category: "Enumerator", log: log)

        if Self.isSystemIdentifier(enumeratedItemIdentifier) {
            logger.info("Providing enumerator for a system defined container.", [.item: enumeratedItemIdentifier])
            serverUrl = account.davFilesUrl
            enumeratedItemMetadata = nil
        } else {
            logger.debug("Providing enumerator for item with identifier.", [.item: enumeratedItemIdentifier])
            enumeratedItemMetadata = dbManager.itemMetadata(
                enumeratedItemIdentifier
            )

            if let enumeratedItemMetadata {
                serverUrl = enumeratedItemMetadata.serverUrl + "/" + enumeratedItemMetadata.fileName
            } else {
                serverUrl = ""
                logger.error("Could not find itemMetadata for file with identifier.", [.item: enumeratedItemIdentifier])
            }
        }

        logger.info("Set up enumerator.", [.account: self.account.ncKitAccount, .url: serverUrl])
        super.init()
    }

    public func invalidate() {
        logger.debug("Enumerator is being invalidated.", [.item: enumeratedItemIdentifier])
    }

    // MARK: - Protocol methods

    public func enumerateItems(for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage) {
        logger.info("Received enumerate items request for enumerator with user", [.account: account.ncKitAccount, .url: serverUrl])
        if enumeratedItemIdentifier == .trashContainer {
            logger.info("Enumerating items in trash.", [.account: account.ncKitAccount, .url: serverUrl])

            Task { [weak self] in
                guard let self else {
                    return
                }

                let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(account: account)

                guard let capabilities, error == .success else {
                    logger.error("Could not acquire capabilities, cannot check trash.", [.error: error])
                    observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                    return
                }

                guard capabilities.files?.undelete == true else {
                    logger.error("Trash is unsupported on server, cannot enumerate items.")
                    observer.finishEnumeratingWithError(NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError))
                    return
                }

                // We only want to list items deleted on the local device.
                // That cannot happen before the initial content enumeration for a file provider domain because the latter does not exist yet.
                // Hence the initial trash content enumeration can be finished with an empty set.
                observer.finishEnumerating(upTo: Self.fileProviderPageforNumPage(1))
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
            logger.error("Enumerator has empty serverUrl - cannot enumerate that!", [.item: enumeratedItemIdentifier])

            let error = NSError.fileProviderErrorForNonExistentItem(withIdentifier: enumeratedItemIdentifier)
            observer.finishEnumeratingWithError(error)
            return
        }

        logger.debug("Enumerating page: \(String(data: page.rawValue, encoding: .utf8) ?? "")", [.account: account.ncKitAccount, .url: serverUrl])

        Task {
            // Do not pass in the NSFileProviderPage default pages, these are not valid Nextcloud
            // pagination tokens
            var pageTotal: Int? = nil
            var pageIndex = 0
            var parsedPage: NSFileProviderPage? = nil

            if page != NSFileProviderPage.initialPageSortedByName as NSFileProviderPage, page != NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage {
                if let enumPageResponse = try? JSONDecoder().decode(EnumeratorPageResponse.self, from: page.rawValue) {
                    if let total = enumPageResponse.total {
                        pageTotal = total
                    }
                    pageIndex = enumPageResponse.index
                    parsedPage = page
                } else {
                    logger.error("Could not parse page")
                }
            }

            // Check server version to determine if pagination should be enabled.
            // Pagination was fixed in Nextcloud 31 (server bug: https://github.com/nextcloud/server/issues/53674)
            // For older servers, we fall back to non-paginated requests.
            // Note: currentCapabilities uses RetrievedCapabilitiesActor which caches capabilities
            // for 30 minutes, so this call is efficient and doesn't make a network request on every enumeration.
            let (_, capabilities, _, _) = await remoteInterface.currentCapabilities(
                account: account,
                options: .init(),
                taskHandler: { _ in }
            )

            let serverMajorVersion = capabilities?.major ?? 0
            let supportsPagination = serverMajorVersion >= 31

            // Enable pagination by passing page settings if server supports it
            let pageSettings: (page: NSFileProviderPage?, index: Int, size: Int)? = supportsPagination ? (
                page: parsedPage,
                index: pageIndex,
                size: pageItemCount
            ) : nil

            let readResult = await Self.readServerUrl(
                serverUrl,
                pageSettings: pageSettings,
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
                logger.error("Finishing enumeration for page with error.", [.account: self.account.ncKitAccount, .error: readError, .url: self.serverUrl])

                // TODO: Refactor for conciseness
                let error = readError?.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier
                ) ?? NSFileProviderError(.cannotSynchronize)
                observer.finishEnumeratingWithError(error)
                return
            }

            guard let metadatas else {
                logger.error("Finishing enumeration with invalid metadata.", [.account: self.account.ncKitAccount, .url: self.serverUrl])
                observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                return
            }

            pageTotal = nextPage?.total ?? pageTotal
            if let rPage = nextPage, let pageTotal, rPage.index * pageItemCount >= pageTotal {
                // Server will sometimes provide a valid next page data even though there are no
                // items to enumerate anymore
                logger.debug("No more items to enumerate, stopping paged enumeration.")
                nextPage = nil
            }

            // Note: Removed unconditional `nextPage = nil` that was disabling pagination
            // This enables proper pagination for large folders (1500+ files) when server supports it

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

    public func enumerateChanges(for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor) {
        logger.debug("Enumerating changes (anchor: \(String(data: anchor.rawValue, encoding: .utf8) ?? "")).", [.url: serverUrl])

        /*
         If this is an enumerator for the working set, then:

         - note the changes in your local database
         - inform the observer about item deletions and updates (modifications + insertions)
         - inform the observer when you have finished enumerating up to a subsequent sync anchor
         */

        if enumeratedItemIdentifier == .workingSet {
            logger.debug("Enumerating changes in working set.", [.account: account])

            let formatter = ISO8601DateFormatter()

            guard let anchorDateString = String(data: anchor.rawValue, encoding: .utf8),
                  let date = formatter.date(from: anchorDateString)
            else {
                logger.error("Could not parse sync anchor \"\(anchor.rawValue)\".")
                observer.finishEnumeratingWithError(NSFileProviderError(.syncAnchorExpired))
                return
            }

            Task {
                await checkMaterializedItemsOnServer()
                let pendingLocalChanges = dbManager.pendingWorkingSetChanges(account: account, since: date)

                completeChangesObserver(
                    observer,
                    anchor: currentAnchor,
                    enumeratedItemIdentifier: enumeratedItemIdentifier,
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: dbManager,
                    newMetadatas: [],
                    updatedMetadatas: pendingLocalChanges.updated,
                    deletedMetadatas: pendingLocalChanges.deleted
                )
            }

            return
        } else if enumeratedItemIdentifier == .trashContainer {
            logger.debug("Enumerating changes in trash.", [.account: account.ncKitAccount])

            Task { [weak self] in
                guard let self else {
                    return
                }

                let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(account: account)

                guard let capabilities, error == .success else {
                    logger.error("Could not acquire capabilities, cannot check trash.", [.error: error])
                    observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                    return
                }

                guard capabilities.files?.undelete == true else {
                    logger.error("Trash is unsupported on server. Cannot enumerate changes.")

                    observer.finishEnumeratingWithError(
                        NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError)
                    )
                    return
                }

                let domain = domain
                let enumeratedItemIdentifier = enumeratedItemIdentifier

                let (_, trashedItems, _, trashReadError) = await remoteInterface.listingTrashAsync(
                    filename: nil,
                    showHiddenFiles: true,
                    account: account.ncKitAccount,
                    options: .init(),
                    taskHandler: { task in
                        if let domain {
                            NSFileProviderManager(for: domain)?.register(
                                task,
                                forItemWithIdentifier: enumeratedItemIdentifier,
                                completionHandler: { _ in }
                            )
                        }
                    }
                )

                guard trashReadError == .success else {
                    let error = trashReadError.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier) ?? NSFileProviderError(.cannotSynchronize)
                    observer.finishEnumeratingWithError(error)
                    return
                }

                await Self.completeChangesObserver(
                    observer,
                    anchor: anchor,
                    account: account,
                    dbManager: dbManager,
                    remoteTrashItems: trashedItems ?? [],
                    log: logger.log
                )
            }

            return
        }

        logger.info("Enumerating changes in item.", [.url: serverUrl])

        // No matter what happens here we finish enumeration in some way, either from the error
        // handling below or from the completeChangesObserver
        // TODO: Move to the sync engine extension
        Task { [weak self] in
            guard let self else {
                return
            }

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
                logger.error("Finished enumerating changes.", [.url: serverUrl, .error: readError])

                let error = readError?.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: enumeratedItemIdentifier) ?? NSFileProviderError(.cannotSynchronize)

                if readError!.isNotFoundError {
                    logger.info("404 error means item no longer exists. Deleting metadata and reporting deletion without error.", [.url: serverUrl])

                    guard let itemMetadata = enumeratedItemMetadata else {
                        logger.error("Invalid enumeratedItemMetadata. Could not delete metadata nor report deletion.")
                        observer.finishEnumeratingWithError(error)
                        return
                    }

                    if itemMetadata.directory {
                        if let deletedDirectoryMetadatas = dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: itemMetadata.ocId) {
                            currentDeletedMetadatas += deletedDirectoryMetadatas
                        } else {
                            logger.error("Something went wrong when recursively deleting directory. It's metadata was not found. Cannot report it as deleted.")
                        }
                    } else {
                        dbManager.deleteItemMetadata(ocId: itemMetadata.ocId)
                    }

                    completeChangesObserver(
                        observer,
                        anchor: anchor,
                        enumeratedItemIdentifier: enumeratedItemIdentifier,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager,
                        newMetadatas: nil,
                        updatedMetadatas: nil,
                        deletedMetadatas: [itemMetadata]
                    )
                    return
                } else if readError!.isNoChangesError { // All is well, just no changed etags
                    logger.info("Error was to say no changed files - not bad error. Finishing change enumeration.")
                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
                    return
                }

                observer.finishEnumeratingWithError(error)
                return
            }

            logger.info("Finished reading remote changes.", [.account: account.ncKitAccount, .url: serverUrl])

            completeChangesObserver(
                observer,
                anchor: anchor,
                enumeratedItemIdentifier: enumeratedItemIdentifier,
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

    private func checkMaterializedItemsOnServer() async {
        logger.debug("Checking materialized items for changes on the server...")

        defer {
            logger.debug("Completed checking materialized items for changes on the server.")
        }

        // Unlike when enumerating items we can't progressively enumerate items as we need to
        // wait to see which items are truly deleted and which have just been moved elsewhere.
        // Visited folders and downloaded files. Sort in terms of their remote URLs.
        // This way we ensure we visit parent folders before their children.
        let materializedItems = dbManager
            .materialisedItemMetadatas(account: account.ncKitAccount)
            .sorted { $0.remotePath().count < $1.remotePath().count }

        var allNewMetadatas = [SendableItemMetadata]()
        var allUpdatedMetadatas = [SendableItemMetadata]()
        var allDeletedMetadatas = [SendableItemMetadata]()
        var examinedItemIds = Set<String>()

        for materializedItem in materializedItems where !examinedItemIds.contains(materializedItem.ocId) {
            guard isLockFileName(materializedItem.fileName) == false else {
                // Skip server requests for locally created lock files.
                // They are not synchronized to the server for real.
                // Thus they can be expected not to be found there.
                // That would also cause their local deletion due to synchronization logic.
                logger.debug("Skipping materialized item in working set check because the name hints a lock file.", [.item: materializedItem, .name: materializedItem.name])
                continue
            }

            let itemRemoteUrl = materializedItem.remotePath()

            let (metadatas, newMetadatas, updatedMetadatas, deletedMetadatas, _, readError) = await Enumerator.readServerUrl(itemRemoteUrl, account: account, remoteInterface: remoteInterface, dbManager: dbManager, depth: materializedItem.directory ? .targetAndDirectChildren : .target, log: logger.log)

            if readError?.errorCode == 404 {
                allDeletedMetadatas.append(materializedItem)
                examinedItemIds.insert(materializedItem.ocId)

                materializedItems.filter {
                    $0.serverUrl.hasPrefix(itemRemoteUrl)
                }.forEach {
                    allDeletedMetadatas.append($0)
                    examinedItemIds.insert($0.ocId)
                }
            } else if let readError, readError != .success {
                logger.error("Finished remote change enumeration of materialized items with error.", [.error: readError])
                return
            } else {
                allDeletedMetadatas += deletedMetadatas ?? []
                allUpdatedMetadatas += updatedMetadatas ?? []
                allNewMetadatas += newMetadatas ?? []

                // Just because we have read child directories metadata doesn't mean we need to in turn scan their children. This is not the case for files.
                var examinedChildFilesAndDeletedItems = Set<String>()

                if let metadatas, let target = metadatas.first {
                    examinedItemIds.insert(target.ocId)

                    if metadatas.count > 1 {
                        examinedChildFilesAndDeletedItems.formUnion(metadatas[1...].filter { !$0.directory }.map(\.ocId))
                    }

                    // If the target is not in the updated metadatas then neither it, nor any of its kids have changed. So skip examining all of them.
                    if !allUpdatedMetadatas.contains(where: { $0.ocId == target.ocId }) {
                        logger.debug("Target has not changed. Skipping children.", [.url: itemRemoteUrl])
                        let materialisedChildren = materializedItems.filter { $0.serverUrl.hasPrefix(itemRemoteUrl) }.map(\.ocId)
                        examinedChildFilesAndDeletedItems.formUnion(materialisedChildren)
                    }

                    // OPTIMIZATION: For any child directories returned in this enumeration, if they haven't changed (etag matches database), mark them as examined so we don't enumerate them separately later.
                    if metadatas.count > 1 {
                        let childDirectories = metadatas[1...].filter(\.directory)

                        for childDir in childDirectories {
                            // Check if this directory is in our materialized items list
                            if let localItem = materializedItems.first(where: { $0.ocId == childDir.ocId }), localItem.etag == childDir.etag {
                                // Directory hasn't changed, mark as examined to skip separate enumeration.
                                logger.debug("Child directory etag unchanged, marking as examined.", [.name: childDir.fileName, .eTag: childDir.etag])
                                examinedChildFilesAndDeletedItems.insert(childDir.ocId)

                                // Also mark any materialized children of this directory as examined.
                                let grandChildren = materializedItems.filter {
                                    $0.serverUrl.hasPrefix(localItem.remotePath())
                                }

                                examinedChildFilesAndDeletedItems.formUnion(grandChildren.map(\.ocId))
                            }
                        }
                    }

                    if let deletedMetadataOcIds = deletedMetadatas?.map(\.ocId) {
                        examinedChildFilesAndDeletedItems.formUnion(deletedMetadataOcIds)
                    }
                }

                examinedItemIds.formUnion(examinedChildFilesAndDeletedItems)
            }
        }

        // Run a check to ensure files deleted in one location are not updated in another (e.g. when moved).
        // The recursive scan provides us with updated/deleted metadatas only on a folder by folder basis; so we need to check we are not simultaneously marking a moved file as deleted and updated.
        var checkedDeletedMetadatas = allDeletedMetadatas

        for updatedMetadata in allUpdatedMetadatas {
            guard let matchingDeletedMetadataIdx = checkedDeletedMetadatas.firstIndex(where: { $0.ocId == updatedMetadata.ocId }) else {
                continue
            }

            checkedDeletedMetadatas.remove(at: matchingDeletedMetadataIdx)
        }

        allDeletedMetadatas = checkedDeletedMetadatas

        for deletedMetadata in allDeletedMetadatas {
            var deleteMarked = deletedMetadata
            deleteMarked.deleted = true
            deleteMarked.syncTime = Date()
            dbManager.addItemMetadata(deleteMarked)
        }

        if allUpdatedMetadatas.isEmpty, allDeletedMetadatas.isEmpty {
            logger.info("No remote changes found in materialized items.")
        }
    }

    static func fileProviderPageforNumPage(_: Int) -> NSFileProviderPage? {
        nil
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
                    logger.info("Did enumerate \(items.count) items. Next page is nil: \(nextPage == nil)")

                    if let nextPage, let nextPageData = try? JSONEncoder().encode(nextPage) {
                        logger.info("Next page: \(String(data: nextPageData, encoding: .utf8) ?? "?")")
                        observer.finishEnumerating(upTo: NSFileProviderPage(nextPageData))
                    } else {
                        observer.finishEnumerating(upTo: nil)
                    }
                }
            } catch let error as NSError { // This error can only mean a missing parent item identifier
                guard handleInvalidParent else {
                    logger.info("Not handling invalid parent in enumeration.")
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
                } catch {
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
            logger.error("Received invalid newMetadatas, updatedMetadatas or deletedMetadatas. Finished enumeration of changes with error.")

            observer.finishEnumeratingWithError(NSError.fileProviderErrorForNonExistentItem(withIdentifier: enumeratedItemIdentifier))

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

        let allFpItemDeletionsIdentifiers = Array(allDeletedMetadatas.map { NSFileProviderItemIdentifier($0.ocId) })

        if !allFpItemDeletionsIdentifiers.isEmpty {
            observer.didDeleteItems(withIdentifiers: allFpItemDeletionsIdentifiers)
        }

        Task { [allUpdatedMetadatas, allDeletedMetadatas] in
            do {
                let updatedItems = try await allUpdatedMetadatas.toFileProviderItems(account: account, remoteInterface: remoteInterface, dbManager: dbManager, log: self.logger.log)

                Task { @MainActor in
                    if !updatedItems.isEmpty {
                        observer.didUpdate(updatedItems)
                    }

                    logger.info("Processed \(updatedItems.count) new or updated metadatas. \(allDeletedMetadatas.count) deleted metadatas.")

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
                } catch {
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
            assertionFailure()
            throw NSError()
        }

        logger.info("Recovering from invalid parent identifier at \(urlToEnumerate)")

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

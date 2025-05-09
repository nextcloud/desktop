/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import FileProvider
import NCDesktopClientSocketKit
import NextcloudKit
import NextcloudFileProviderKit
import OSLog

@objc class FileProviderExtension: NSObject, NSFileProviderReplicatedExtension {
    let domain: NSFileProviderDomain
    let ncKit = NextcloudKit.shared
    let appGroupIdentifier = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix") as? String
    var ncAccount: Account?
    var dbManager: FilesDatabaseManager?
    var changeObserver: RemoteChangeObserver?
    lazy var ncKitBackground = NKBackground(nkCommonInstance: ncKit.nkCommonInstance)
    lazy var socketClient: LocalSocketClient? = {
        guard let containerUrl = pathForAppGroupContainer() else {
            Logger.fileProviderExtension.critical("Won't start socket client, no container url")
            return nil;
        }

        let socketPath = containerUrl.appendingPathComponent(
            ".fileprovidersocket", conformingTo: .archive)
        let lineProcessor = FileProviderSocketLineProcessor(delegate: self)
        return LocalSocketClient(socketPath: socketPath.path, lineProcessor: lineProcessor)
    }()

    var syncActions = Set<UUID>()
    var errorActions = Set<UUID>()
    var actionsLock = NSLock()

    // Whether or not we are going to recursively scan new folders when they are discovered.
    // Apple's recommendation is that we should always scan the file hierarchy fully.
    // This does lead to long load times when a file provider domain is initially configured.
    // We can instead do a fast enumeration where we only scan folders as the user navigates through
    // them, thereby avoiding this issue; the trade-off is that we will be unable to detect
    // materialised file moves to unexplored folders, therefore deleting the item when we could have
    // just moved it instead.
    //
    // Since it's not desirable to cancel a long recursive enumeration half-way through, we do the
    // fast enumeration by default. We prompt the user on the client side to run a proper, full
    // enumeration if they want for safety.
    lazy var config = FileProviderConfig(domainIdentifier: domain.identifier)

    required init(domain: NSFileProviderDomain) {
        // The containing application must create a domain using 
        // `NSFileProviderManager.add(_:, completionHandler:)`. The system will then launch the
        // application extension process, call `FileProviderExtension.init(domain:)` to instantiate
        // the extension for that domain, and call methods on the instance.
        self.domain = domain
        super.init()
        socketClient?.start()
    }

    func invalidate() {
        // TODO: cleanup any resources
        Logger.fileProviderExtension.debug(
            "Extension for domain \(self.domain.displayName, privacy: .public) is being torn down"
        )
    }

    func insertSyncAction(_ actionId: UUID) {
        actionsLock.lock()
        let oldActions = syncActions
        syncActions.insert(actionId)
        actionsLock.unlock()
        updatedSyncStateReporting(oldActions: oldActions)
    }

    func insertErrorAction(_ actionId: UUID) {
        actionsLock.lock()
        let oldActions = syncActions
        syncActions.remove(actionId)
        errorActions.insert(actionId)
        actionsLock.unlock()
        updatedSyncStateReporting(oldActions: oldActions)
    }

    func removeSyncAction(_ actionId: UUID) {
        actionsLock.lock()
        let oldActions = syncActions
        syncActions.remove(actionId)
        errorActions.remove(actionId)
        actionsLock.unlock()
        updatedSyncStateReporting(oldActions: oldActions)
    }

    // MARK: - NSFileProviderReplicatedExtension protocol methods

    func item(
        for identifier: NSFileProviderItemIdentifier, 
        request _: NSFileProviderRequest,
        completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void
    ) -> Progress {
        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                """
                Not fetching item for identifier: \(identifier.rawValue, privacy: .public)
                as account not set up yet.
                """
            )
            completionHandler(nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }
        guard let dbManager else {
            Logger.fileProviderExtension.error(
                """
                Not fetching item for identifier: \(identifier.rawValue, privacy: .public)
                    as database is unreachable
                """
            )
            completionHandler(nil, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        if let item = Item.storedItem(
            identifier: identifier, account: ncAccount, remoteInterface: ncKit, dbManager: dbManager
        ) {
            completionHandler(item, nil)
        } else {
            completionHandler(
                nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier)
            )
        }
        return Progress()
    }

    func fetchContents(
        for itemIdentifier: NSFileProviderItemIdentifier,
        version requestedVersion: NSFileProviderItemVersion?, 
        request: NSFileProviderRequest,
        completionHandler: @escaping (URL?, NSFileProviderItem?, Error?) -> Void
    ) -> Progress {
        let actionId = UUID()
        insertSyncAction(actionId)

        Logger.fileProviderExtension.debug(
            "Received request to fetch contents of item with identifier: \(itemIdentifier.rawValue, privacy: .public)"
        )

        guard requestedVersion == nil else {
            // TODO: Add proper support for file versioning
            Logger.fileProviderExtension.error(
                "Can't return contents for a specific version as this is not supported."
            )
            insertErrorAction(actionId)
            completionHandler(
                nil, 
                nil,
                NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError)
            )
            return Progress()
        }

        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                """
                Not fetching contents for item: \(itemIdentifier.rawValue, privacy: .public)
                as account not set up yet.
                """
            )
            insertErrorAction(actionId)
            completionHandler(nil, nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }
        guard let dbManager else {
            Logger.fileProviderExtension.error(
                """
                Not fetching contents for item: \(itemIdentifier.rawValue, privacy: .public)
                    as database is unreachable
                """
            )
            completionHandler(nil, nil, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        guard let item = Item.storedItem(
            identifier: itemIdentifier,
            account: ncAccount,
            remoteInterface: ncKit,
            dbManager: dbManager
        ) else {
            Logger.fileProviderExtension.error(
                """
                Not fetching contents for item: \(itemIdentifier.rawValue, privacy: .public)
                as item not found.
                """
            )
            completionHandler(
                nil,
                nil,
                NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier)
            )
            insertErrorAction(actionId)
            return Progress()
        }

        let progress = Progress()
        Task {
            let (localUrl, updatedItem, error) = await item.fetchContents(
                domain: self.domain, progress: progress, dbManager: dbManager
            )
            removeSyncAction(actionId)
            completionHandler(localUrl, updatedItem, error)
        }
        return progress
    }

    func createItem(
        basedOn itemTemplate: NSFileProviderItem, 
        fields: NSFileProviderItemFields,
        contents url: URL?, 
        options: NSFileProviderCreateItemOptions = [],
        request: NSFileProviderRequest,
        completionHandler: @escaping ( 
            NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?
        ) -> Void
    ) -> Progress {
        let actionId = UUID()
        insertSyncAction(actionId)

        let tempId = itemTemplate.itemIdentifier.rawValue
        Logger.fileProviderExtension.debug(
            """
            Received create item request for item with identifier: \(tempId, privacy: .public)
            and filename: \(itemTemplate.filename, privacy: .public)
            """
        )

        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                """
                Not creating item: \(itemTemplate.itemIdentifier.rawValue, privacy: .public)
                as account not set up yet
                """
            )
            insertErrorAction(actionId)
            completionHandler(itemTemplate, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            Logger.fileProviderExtension.error(
                """
                Not creating item: \(itemTemplate.itemIdentifier.rawValue, privacy: .public)
                    as database is unreachable
                """
            )
            insertErrorAction(actionId)
            completionHandler(itemTemplate, [], false, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        let progress = Progress()
        Task {
            let (item, error) = await Item.create(
                basedOn: itemTemplate,
                fields: fields,
                contents: url,
                request: request,
                domain: self.domain,
                account: ncAccount,
                remoteInterface: ncKit,
                progress: progress,
                dbManager: dbManager
            )

            if error != nil {
                insertErrorAction(actionId)
                signalEnumerator(completionHandler: { _ in })
            } else {
                removeSyncAction(actionId)
            }

            completionHandler(
                item ?? itemTemplate,
                NSFileProviderItemFields(),
                false,
                error
            )
        }
        return progress
    }

    func modifyItem(
        _ item: NSFileProviderItem, 
        baseVersion: NSFileProviderItemVersion,
        changedFields: NSFileProviderItemFields, 
        contents newContents: URL?,
        options: NSFileProviderModifyItemOptions = [], 
        request: NSFileProviderRequest,
        completionHandler: @escaping (
            NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?
        ) -> Void
    ) -> Progress {
        // An item was modified on disk, process the item's modification
        // TODO: Handle finder things like tags, other possible item changed fields
        let actionId = UUID()
        insertSyncAction(actionId)

        let identifier = item.itemIdentifier
        let ocId = identifier.rawValue
        Logger.fileProviderExtension.debug(
            """
            Received modify item request for item with identifier: \(ocId, privacy: .public)
            and filename: \(item.filename, privacy: .public)
            """
        )

        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not modifying item: \(ocId, privacy: .public) as account not set up yet."
            )
            insertErrorAction(actionId)
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }


        guard let dbManager else {
            Logger.fileProviderExtension.error(
                """
                Not modifying item: \(ocId, privacy: .public)
                    with filename: \(item.filename, privacy: .public)
                    as database is unreachable
                """
            )
            insertErrorAction(actionId)
            completionHandler(item, [], false, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        guard let existingItem = Item.storedItem(
            identifier: identifier, account: ncAccount, remoteInterface: ncKit, dbManager: dbManager
        ) else {
            Logger.fileProviderExtension.error(
                "Not modifying item: \(ocId, privacy: .public) as item not found."
            )
            insertErrorAction(actionId)
            completionHandler(
                item,
                [],
                false,
                NSError.fileProviderErrorForNonExistentItem(withIdentifier: item.itemIdentifier)
            )
            return Progress()
        }

        let progress = Progress()
        Task {
            let (modifiedItem, error) = await existingItem.modify(
                itemTarget: item,
                baseVersion: baseVersion,
                changedFields: changedFields,
                contents: newContents,
                options: options,
                request: request,
                domain: domain,
                progress: progress,
                dbManager: dbManager
            )

            if error != nil {
                insertErrorAction(actionId)
                signalEnumerator(completionHandler: { _ in })
            } else {
                removeSyncAction(actionId)
            }

            completionHandler(modifiedItem ?? item, [], false, error)
        }
        return progress
    }

    func deleteItem(
        identifier: NSFileProviderItemIdentifier, 
        baseVersion _: NSFileProviderItemVersion,
        options _: NSFileProviderDeleteItemOptions = [], 
        request _: NSFileProviderRequest,
        completionHandler: @escaping (Error?) -> Void
    ) -> Progress {
        let actionId = UUID()
        insertSyncAction(actionId)

        Logger.fileProviderExtension.debug(
            "Received delete request for item: \(identifier.rawValue, privacy: .public)"
        )

        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not deleting item \(identifier.rawValue, privacy: .public), account not set up yet"
            )
            insertErrorAction(actionId)
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            Logger.fileProviderExtension.error(
                "Not deleting item \(identifier.rawValue, privacy: .public), db manager unavailable"
            )
            insertErrorAction(actionId)
            completionHandler(NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        guard let item = Item.storedItem(
            identifier: identifier, account: ncAccount, remoteInterface: ncKit, dbManager: dbManager
        ) else {
            Logger.fileProviderExtension.error(
                "Not deleting item \(identifier.rawValue, privacy: .public), item not found"
            )
            insertErrorAction(actionId)
            completionHandler(
                NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier)
            )
            return Progress()
        }

        let progress = Progress(totalUnitCount: 1)
        guard config.trashDeletionEnabled || item.parentItemIdentifier != .trashContainer else {
            Logger.fileProviderExtension.warning(
                """
                System requested deletion of item in trash, but deleting trash items is disabled.
                    item: \(item.filename, privacy: .public)
                """
            )
            completionHandler(NSError.fileProviderErrorForRejectedDeletion(of: item))
            return progress
        }

        Task {
            let error = await item.delete(dbManager: dbManager)
            if error != nil {
                insertErrorAction(actionId)
                signalEnumerator(completionHandler: { _ in })
            } else {
                removeSyncAction(actionId)
            }
            progress.completedUnitCount = 1
            completionHandler(error)
        }
        return progress
    }

    func enumerator(
        for containerItemIdentifier: NSFileProviderItemIdentifier, request _: NSFileProviderRequest
    ) throws -> NSFileProviderEnumerator {
        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                """
                Not providing enumerator for container
                    with identifier \(containerItemIdentifier.rawValue, privacy: .public) yet
                    as account not set up
                """
            )
            throw NSFileProviderError(.notAuthenticated)
        }

        guard let dbManager else {
            Logger.fileProviderExtension.error(
                """
                Not providing enumerator for container
                    with identifier \(containerItemIdentifier.rawValue, privacy: .public) yet
                    as db manager is unavailable
                """
            )
            throw NSFileProviderError(.cannotSynchronize)
        }

        return Enumerator(
            enumeratedItemIdentifier: containerItemIdentifier,
            account: ncAccount,
            remoteInterface: ncKit,
            dbManager: dbManager,
            domain: domain,
            fastEnumeration: config.fastEnumerationEnabled
        )
    }

    func materializedItemsDidChange(completionHandler: @escaping () -> Void) {
        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not purging stale local file metadatas, account not set up")
            completionHandler()
            return
        }

        guard let dbManager else {
            Logger.fileProviderExtension.error(
                """
                Not purging stale local file metadatas.
                    db manager unabilable for domain: \(self.domain.displayName, privacy: .public)
                """
            )
            completionHandler()
            return
        }

        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error(
                "Could not get file provider manager for domain: \(self.domain.displayName, privacy: .public)"
            )
            completionHandler()
            return
        }

        let materialisedEnumerator = fpManager.enumeratorForMaterializedItems()
        let materialisedObserver = MaterialisedEnumerationObserver(
            ncKitAccount: ncAccount.ncKitAccount, dbManager: dbManager
        ) { _ in
            completionHandler()
        }
        let startingPage = NSFileProviderPage(NSFileProviderPage.initialPageSortedByName as Data)

        materialisedEnumerator.enumerateItems(for: materialisedObserver, startingAt: startingPage)
    }

    // MARK: - Helper functions

    func signalEnumerator(completionHandler: @escaping (_ error: Error?) -> Void) {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error(
                "Could not get file provider manager for domain, could not signal enumerator. This might lead to future conflicts."
            )
            return
        }

        fpManager.signalEnumerator(for: .workingSet, completionHandler: completionHandler)
    }
}

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

    let keychain: Keychain
    let log: any FileProviderLogging
    let logger: FileProviderLogger

    ///
    /// NextcloudKit instance used by this file provider extension object.
    ///
    let ncKit: NextcloudKit

    let appGroupIdentifier = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix") as? String
    var ncAccount: Account?
    var dbManager: FilesDatabaseManager?
    var changeObserver: RemoteChangeObserver?
    var ignoredFiles: IgnoredFilesMatcher?
    lazy var ncKitBackground = NKBackground(nkCommonInstance: ncKit.nkCommonInstance)
    lazy var socketClient: LocalSocketClient? = {
        guard let containerUrl = pathForAppGroupContainer() else {
            logger.fault("Won't start socket client, no container URL available!")
            return nil;
        }

        let socketPath = containerUrl.appendingPathComponent(
            ".fileprovidersocket", conformingTo: .archive)
        let lineProcessor = FileProviderSocketLineProcessor(delegate: self, log: log)
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
    lazy var config = FileProviderDomainDefaults(identifier: domain.identifier, log: log)

    required init(domain: NSFileProviderDomain) {
        // The containing application must create a domain using 
        // `NSFileProviderManager.add(_:, completionHandler:)`. The system will then launch the
        // application extension process, call `FileProviderExtension.init(domain:)` to instantiate
        // the extension for that domain, and call methods on the instance.
        self.domain = domain

        // Set up logging.
        self.log = FileProviderLog(fileProviderDomainIdentifier: domain.identifier)
        self.logger = FileProviderLogger(category: "FileProviderExtension", log: log)
        logger.debug("Initializing with domain identifier: \(domain.identifier.rawValue)")

        // Set up NextcloudKit.
        self.ncKit = NextcloudKit.shared

        #if DEBUG
        NKLogFileManager.configure(logLevel: .verbose)
        #else
        NKLogFileManager.configure(logLevel: .normal)
        #endif

        logger.info("Current NextcloudKit log file URL: \(NKLogFileManager.shared.currentLogFileURL().absoluteString)")

        self.keychain = Keychain(log: log)

        super.init()
        socketClient?.start()
    }

    func invalidate() {
        logger.debug("File provider extension process is being invalidated.")
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

    func item(for identifier: NSFileProviderItemIdentifier, request: NSFileProviderRequest, completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void) -> Progress {
        logger.debug("Received request for item.", [.item: identifier, .request: request])

        guard let ncAccount else {
            logger.error("Not fetching item because account not set up yet.", [.item: identifier])
            completionHandler(nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }
        
        guard let dbManager else {
            logger.error("Not fetching item because database is unavailable.", [.item: identifier])
            completionHandler(nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let progress = Progress()
        Task {
            progress.totalUnitCount = 1
            if let item = await Item.storedItem(
                identifier: identifier,
                account: ncAccount,
                remoteInterface: ncKit,
                dbManager: dbManager,
                log: log
            ) {
                progress.completedUnitCount = 1
                completionHandler(item, nil)
            } else {
                completionHandler(
                    nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier)
                )
            }
        }
        return progress
    }

    func fetchContents(
        for itemIdentifier: NSFileProviderItemIdentifier,
        version requestedVersion: NSFileProviderItemVersion?, 
        request: NSFileProviderRequest,
        completionHandler: @escaping (URL?, NSFileProviderItem?, Error?) -> Void
    ) -> Progress {
        let actionId = UUID()
        insertSyncAction(actionId)
        logger.debug("Received request to fetch contents of item.", [.item: itemIdentifier, .request: request])

        guard requestedVersion == nil else {
            // TODO: Add proper support for file versioning
            logger.error("Can't return contents for a specific version as this is not supported.", [.item: itemIdentifier])
            insertErrorAction(actionId)
            completionHandler(
                nil, 
                nil,
                NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError)
            )
            return Progress()
        }

        guard let ncAccount else {
            logger.error("Not fetching contents for item because account not set up yet.", [.item: itemIdentifier])
            insertErrorAction(actionId)
            completionHandler(nil, nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            logger.error("Not fetching contents for item because database is unavailable.", [.item: itemIdentifier])
            completionHandler(nil, nil, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }


        let progress = Progress()
        Task {
            guard let item = await Item.storedItem(
                identifier: itemIdentifier,
                account: ncAccount,
                remoteInterface: ncKit,
                dbManager: dbManager,
                log: log
            ) else {
                logger.error("Not fetching contents for item because item was not found.", [.item: itemIdentifier])

                completionHandler(
                    nil,
                    nil,
                    NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier)
                )
                insertErrorAction(actionId)
                return
            }

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
        logger.debug("Received request to create item.", [.item: itemTemplate, .name: itemTemplate.filename, .request: request])

        guard let ncAccount else {
            logger.error(
                """
                Not creating item: \(itemTemplate.itemIdentifier.rawValue)
                as account not set up yet
                """
            )
            insertErrorAction(actionId)
            completionHandler(itemTemplate, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let ignoredFiles else {
            logger.error("Not creating item for identifier: \(itemTemplate.itemIdentifier.rawValue) as ignore list not set up yet.")
            insertErrorAction(actionId)
            completionHandler(itemTemplate, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            logger.error("Not creating item because database is unavailable.", [.item: itemTemplate.itemIdentifier])
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
                ignoredFiles: ignoredFiles,
                progress: progress,
                dbManager: dbManager,
                log: log
            )

            if error != nil {
                insertErrorAction(actionId)
                signalEnumerator(completionHandler: { _ in })
            } else {
                removeSyncAction(actionId)
            }

            logger.debug("Calling item creation completion handler.", [.item: item?.itemIdentifier, .name: item?.filename, .error: error])
            
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
        logger.debug("Received request to modify item.", [.item: item, .request: request])

        guard let ncAccount else {
            logger.error("Not modifying item because account not set up yet.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let ignoredFiles else {
            logger.error("Not modifying item because ignore list not set up yet.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }


        guard let dbManager else {
            logger.error("Not modifying item because the database is unavailable.")
            insertErrorAction(actionId)
            completionHandler(item, [], false, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        let progress = Progress()
        
        Task {
            guard let existingItem = await Item.storedItem(
                identifier: identifier,
                account: ncAccount,
                remoteInterface: ncKit,
                dbManager: dbManager,
                log: log
            ) else {
                logger.error("Not modifying item because it was not found.", [.item: identifier])
                insertErrorAction(actionId)
                
                completionHandler(
                    item,
                    [],
                    false,
                    NSError.fileProviderErrorForNonExistentItem(withIdentifier: item.itemIdentifier)
                )
                
                return
            }
            
            let (modifiedItem, error) = await existingItem.modify(
                itemTarget: item,
                baseVersion: baseVersion,
                changedFields: changedFields,
                contents: newContents,
                options: options,
                request: request,
                ignoredFiles: ignoredFiles,
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

            logger.debug("Calling item modification completion handler.", [.item: item.itemIdentifier, .name: item.filename, .error: error])
            completionHandler(modifiedItem ?? item, [], false, error)
        }
        
        return progress
    }

    func deleteItem(
        identifier: NSFileProviderItemIdentifier, 
        baseVersion _: NSFileProviderItemVersion,
        options _: NSFileProviderDeleteItemOptions = [], 
        request: NSFileProviderRequest,
        completionHandler: @escaping (Error?) -> Void
    ) -> Progress {
        let actionId = UUID()
        insertSyncAction(actionId)

        logger.debug("Received request to delete item.", [.item: identifier, .request: request])

        guard let ncAccount else {
            logger.error("Not deleting item \(identifier.rawValue), account not set up yet")
            insertErrorAction(actionId)
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let ignoredFiles else {
            logger.error("Not deleting \(identifier.rawValue), ignore list not received")
            insertErrorAction(actionId)
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            logger.error("Not deleting item \(identifier.rawValue), db manager unavailable")
            insertErrorAction(actionId)
            completionHandler(NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        let progress = Progress(totalUnitCount: 1)
        Task {
            guard let item = await Item.storedItem(
                identifier: identifier,
                account: ncAccount,
                remoteInterface: ncKit,
                dbManager: dbManager,
                log: log
            ) else {
                logger.error("Not deleting item because it was not found.", [.item: identifier])
                insertErrorAction(actionId)
                completionHandler(NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier))
                return
            }
            
            logger.debug("Found item for identifier.", [.item: identifier, .name: item.filename])

            guard config.trashDeletionEnabled || item.parentItemIdentifier != .trashContainer else {
                logger.info("System requested deletion of item in trash, but deleting trash items is disabled. item: \(item.filename)")
                removeSyncAction(actionId)
                completionHandler(NSError.fileProviderErrorForRejectedDeletion(of: item))
                return
            }
            
            let error = await item.delete(domain: domain, ignoredFiles: ignoredFiles, dbManager: dbManager)
            
            if error != nil {
                insertErrorAction(actionId)
                signalEnumerator(completionHandler: { _ in })
            } else {
                removeSyncAction(actionId)
            }
            
            progress.completedUnitCount = 1
            logger.debug("Calling item deletion completion handler.", [.item: identifier, .name: item.filename, .error: error])
            completionHandler(error)
        }
        return progress
    }

    func enumerator(for containerItemIdentifier: NSFileProviderItemIdentifier, request: NSFileProviderRequest) throws -> NSFileProviderEnumerator {
        logger.debug("System requested enumerator.", [.item: containerItemIdentifier])

        guard let ncAccount else {
            logger.error("Not providing enumerator for container with identifier \(containerItemIdentifier.rawValue) yet as account not set up")
            throw NSFileProviderError(.notAuthenticated)
        }

        guard let dbManager else {
            logger.error("Not providing enumerator for container with identifier \(containerItemIdentifier.rawValue) yet as db manager is unavailable")
            throw NSFileProviderError(.cannotSynchronize)
        }

        return Enumerator(
            enumeratedItemIdentifier: containerItemIdentifier,
            account: ncAccount,
            remoteInterface: ncKit,
            dbManager: dbManager,
            domain: domain,
            log: log
        )
    }

    func materializedItemsDidChange(completionHandler: @escaping () -> Void) {
        guard let ncAccount else {
            logger.error("Not purging stale local file metadatas, account not set up")
            completionHandler()
            return
        }

        guard let dbManager else {
            logger.error("Not purging stale local file metadatas. db manager unabilable for domain: \(self.domain.displayName)")
            completionHandler()
            return
        }

        guard let fpManager = NSFileProviderManager(for: domain) else {
            logger.error("Could not get file provider manager for domain: \(self.domain.displayName)")
            completionHandler()
            return
        }

        let materialisedEnumerator = fpManager.enumeratorForMaterializedItems()
        let materialisedObserver = MaterialisedEnumerationObserver(ncKitAccount: ncAccount.ncKitAccount, dbManager: dbManager, log: log) { _, _ in
            completionHandler()
        }
        let startingPage = NSFileProviderPage(NSFileProviderPage.initialPageSortedByName as Data)

        materialisedEnumerator.enumerateItems(for: materialisedObserver, startingAt: startingPage)
    }

    // MARK: - Helper functions

    func signalEnumerator(completionHandler: @escaping (_ error: Error?) -> Void) {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            logger.error("Could not get file provider manager for domain, could not signal enumerator. This might lead to future conflicts.")
            return
        }

        fpManager.signalEnumerator(for: .workingSet, completionHandler: completionHandler)
    }
}

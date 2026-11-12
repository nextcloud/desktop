//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import NextcloudFileProviderXPC
import NextcloudKit
import OSLog

///
/// The file provider replicated extension implementation.
///
/// `public` so the runtime can locate this principal class via the package's module name
/// (referenced from `Info.plist`'s `NSExtensionPrincipalClass`).
///
@objc public final class FileProviderExtension: NSObject, NSFileProviderReplicatedExtension, @unchecked Sendable {
    ///
    /// The file provider domain managed by this file provider extension implementation.
    ///
    let domain: NSFileProviderDomain
    /// File Provider may report a completed download after account setup replaces the database manager.
    private let pendingMaterializations = PendingMaterializationRegistry()

    ///
    /// The password of the account, cached for when the main app cannot hand it over.
    ///
    let keychain: Keychain
    let log: any FileProviderLogging
    let logger: FileProviderLogger

    ///
    /// The file provider manager for the domain managed by this extension implementation.
    ///
    let manager: NSFileProviderManager?
    private let enumerateMaterializedItems: ((any NSFileProviderEnumerationObserver, NSFileProviderPage) -> Void)?

    ///
    /// Observation of the process-global `blockSync` user default, used only to notice when synchronization is unblocked.
    ///
    /// See ``observeBlockSync()`` for why the gates do not depend on this.
    ///
    var blockSyncObservation: NSKeyValueObservation?

    // MARK: XPC

    ///
    /// The remote object proxy to interact with the app.
    ///
    /// This is updated by the `NSXPCListenerDelegate` implementation.
    ///
    var app: (any AppProtocol)?

    ///
    /// Connections established by the `NSXPCListenerDelegate` extension on this type.
    ///
    /// The individual interr
    ///
    var connections = Set<NSXPCConnection>()

    public var listener = NSXPCListener.anonymous()
    public let serviceName = NSFileProviderServiceName("com.nextcloud.desktopclient.ClientCommunicationService")

    ///
    /// NextcloudKit instance used by this file provider extension object.
    ///
    let ncKit: NextcloudKit
    private let databaseDirectory: URL?
    private let authenticationRetryDelays: [UInt64]
    private let makeDAVValidationSession: (NextcloudKit, Account, String) -> String
    var thumbnailFetcher = NextcloudFileProviderKit.fetchThumbnails

    /// A request captures this manager once and uses the account it was opened with.
    var dbManager: FilesDatabaseManager? {
        setupLock.withLock { publishedDatabaseManager }
    }

    /// Capture the current account and register the request before setup can stop admission.
    func beginSyncOperation(
        onStop: (@Sendable () -> Void)? = nil,
        resumeWrite: (@Sendable () -> Void)? = nil
    ) throws -> (databaseManager: FilesDatabaseManager, operation: SyncOperations.Operation) {
        try setupLock.withLock {
            guard let manager = publishedDatabaseManager else { throw NSFileProviderError(.notAuthenticated) }
            guard let operation = manager.syncOperations.begin(onStop: onStop, resumeWrite: resumeWrite) else {
                throw NSFileProviderError(.serverUnreachable)
            }
            return (manager, operation)
        }
    }

    /// A cancelled read can return after setup sent its initial retry signal.
    func retryIfStopped(_ operation: SyncOperations.Operation, completion: (Error) -> Void) -> Bool {
        guard !operation.beginDelivery() else { return false }
        completion(NSFileProviderError(.serverUnreachable))
        if dbManager?.syncOperations.isAcceptingWork == true {
            signalFileProviderToRetryUnreachableRequests()
        }
        return true
    }

    var ignoredFiles: IgnoredFilesMatcher?
    lazy var ncKitBackground = NKBackground(nkCommonInstance: ncKit.nkCommonInstance)

    var syncActions = Set<UUID>()
    var errorActions = Set<UUID>()
    var actionsLock = NSLock()

    // Serialization state for `setupDomainAccount(…)`. See the method for details.
    private let setupLock = NSLock()
    private var publishedDatabaseManager: FilesDatabaseManager?
    private var pendingAccount: Account?
    private var materializedItemsRefresh: SyncOperations.Operation?
    private var materializedItemsRefreshPending = false
    private var setupChain: Task<Void, Never> = Task {}

    ///
    /// Incremented by ``removeAccount()``, so setups queued or running before it neither store nor publish their account.
    ///
    private(set) var setupGeneration = 0

    // Waiters parked in `awaitDatabaseManager(…)` until account setup publishes the manager.
    let accountReadyLock = NSLock()
    private(set) var accountReadyWaiters = [UUID: CheckedContinuation<Void, Never>]()

    /// Whether or not we are going to recursively scan new folders when they are discovered.
    /// Apple's recommendation is that we should always scan the file hierarchy fully.
    /// This does lead to long load times when a file provider domain is initially configured.
    /// We can instead do a fast enumeration where we only scan folders as the user navigates through
    /// them, thereby avoiding this issue; the trade-off is that we will be unable to detect
    /// materialized file moves to unexplored folders, therefore deleting the item when we could have
    /// just moved it instead.
    ///
    /// Since it's not desirable to cancel a long recursive enumeration half-way through, we do the
    /// fast enumeration by default. We prompt the user on the client side to run a proper, full
    /// enumeration if they want for safety.
    lazy var config = FileProviderDomainDefaults(identifier: domain.identifier, log: log)

    /// NextcloudKit's logger is configured once per process. Its configuration is not thread-safe, and a process hosts one extension per domain, so instances created concurrently, as the tests do, must not configure it again. The static initialiser runs once, before the first instance logs through it.
    private static let nextcloudKitLoggingConfiguration: Void = {
        #if DEBUG
            NKLogFileManager.configure(logLevel: .verbose)
        #else
            NKLogFileManager.configure(logLevel: .normal)
        #endif
    }()

    public required convenience init(domain: NSFileProviderDomain) {
        // The containing application must create a domain using
        // `NSFileProviderManager.add(_:, completionHandler:)`. The system will then launch the
        // application extension process, call `FileProviderExtension.init(domain:)` to instantiate
        // the extension for that domain, and call methods on the instance.
        self.init(domain: domain, ncKit: .shared)
    }

    init(
        domain: NSFileProviderDomain,
        ncKit: NextcloudKit,
        databaseDirectory: URL? = nil,
        authenticationRetryDelays: [UInt64] = AuthenticationRetryDelays,
        makeDAVValidationSession: @escaping (NextcloudKit, Account, String) -> String = {
            $0.makeDAVValidationSession(account: $1, userAgent: $2)
        },
        enumerateMaterializedItems: ((any NSFileProviderEnumerationObserver, NSFileProviderPage) -> Void)? = nil
    ) {
        self.domain = domain
        self.databaseDirectory = databaseDirectory
        self.authenticationRetryDelays = authenticationRetryDelays
        self.makeDAVValidationSession = makeDAVValidationSession
        self.enumerateMaterializedItems = enumerateMaterializedItems
        manager = NSFileProviderManager(for: domain)

        // Set up logging.
        log = FileProviderLog(fileProviderDomainIdentifier: domain.identifier)
        logger = FileProviderLogger(category: "FileProviderExtension", log: log)
        logger.debug("Initializing with domain identifier.", [.domain: domain.identifier.rawValue])

        // Set up NextcloudKit.
        self.ncKit = ncKit
        _ = Self.nextcloudKitLoggingConfiguration
        logger.info("NextcloudKit logging configured.", [.url: NKLogFileManager.shared.currentLogFileURL()])
        keychain = Keychain(domain: domain, log: log)
        super.init()
        observeBlockSync()
    }

    public func invalidate() {
        logger.debug("File provider extension process is being invalidated.")
        blockSyncObservation?.invalidate()
        blockSyncObservation = nil
        // The running drain nudges through this instance's manager and database, so it stops; what is still pending waits for the next enqueue's drain.
        AncestorRefreshCoalescer.shared(for: domain.identifier).cancel()
        dbManager?.checkpointForShutdown()
    }

    func insertSyncAction(_ actionId: UUID) {
        logger.debug("Inserting synchronization action.", [.item: actionId])

        actionsLock.lock()
        let oldActions = syncActions
        syncActions.insert(actionId)
        actionsLock.unlock()
        updatedSyncStateReporting(oldActions: oldActions)
    }

    func insertErrorAction(_ actionId: UUID) {
        logger.debug("Inserting error action.", [.item: actionId])

        actionsLock.lock()
        let oldActions = syncActions
        syncActions.remove(actionId)
        errorActions.insert(actionId)
        actionsLock.unlock()
        updatedSyncStateReporting(oldActions: oldActions)
    }

    func removeSyncAction(_ actionId: UUID) {
        logger.debug("Removing synchronization action.", [.item: actionId])

        actionsLock.lock()
        let oldActions = syncActions
        syncActions.remove(actionId)
        errorActions.remove(actionId)
        actionsLock.unlock()
        updatedSyncStateReporting(oldActions: oldActions)
    }

    func completeSyncAction(_ actionId: UUID, error: Error?) {
        if error == nil || (error as? CocoaError)?.code == .userCancelled {
            removeSyncAction(actionId)
        } else {
            insertErrorAction(actionId)
            signalEnumerator(completionHandler: { _ in })
        }
    }

    // MARK: - NSFileProviderReplicatedExtension protocol methods

    public func item(for identifier: NSFileProviderItemIdentifier, request _: NSFileProviderRequest, completionHandler: @Sendable @escaping (NSFileProviderItem?, Error?) -> Void) -> Progress {
        logger.debug("Received request for item.", [.item: identifier])

        let progress = Progress(totalUnitCount: 1)

        Task {
            // Same startup race as `fetchContents`, where answering `notAuthenticated` would
            // make the framework treat the item as unreachable rather than not-ready-yet.
            let (dbManager, operation): (FilesDatabaseManager, SyncOperations.Operation)
            do {
                _ = try await awaitDatabaseManager()
                (dbManager, operation) = try beginSyncOperation()
            } catch {
                completionHandler(nil, error)
                return
            }
            operation.run { [self] in
                do {
                    let item = try await Item.lookupItemAndFindParentIdentifier(
                        identifier: identifier,
                        account: dbManager.account,
                        remoteInterface: ncKit,
                        dbManager: dbManager,
                        includeDeleted: false,
                        allowsRemoteLookup: !blockSync,
                        taskHandler: { operation.registerReadTask($0) },
                        log: log
                    )
                    guard !retryIfStopped(operation, completion: { completionHandler(nil, $0) }) else { return }
                    progress.completedUnitCount = 1
                    completionHandler(item, nil)
                } catch {
                    guard !retryIfStopped(operation, completion: { completionHandler(nil, $0) }) else { return }
                    completionHandler(nil, error)
                }
            }
        }

        return progress
    }

    public func fetchContents(for itemIdentifier: NSFileProviderItemIdentifier, version requestedVersion: NSFileProviderItemVersion?, request _: NSFileProviderRequest, completionHandler: @Sendable @escaping (URL?, NSFileProviderItem?, Error?) -> Void) -> Progress {
        guard !blockSync else {
            logger.info("Not fetching contents of item because synchronization is blocked.", [.item: itemIdentifier])
            completionHandler(nil, nil, NSFileProviderError(.serverUnreachable))

            return Progress()
        }

        let actionId = UUID()
        insertSyncAction(actionId)
        logger.debug("Received request to fetch contents of item.", [.item: itemIdentifier])

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

        let progress = Progress()

        Task {
            await NetworkOperationCancellation(log: log).run(progress: progress) { cancellation in
                // File Provider can request contents before the main app has supplied the account.
                do {
                    _ = try await self.awaitDatabaseManager()
                    let (dbManager, operation) = try self.beginSyncOperation(onStop: { cancellation.cancel() })
                    defer { operation.finish() }
                    let (localUrl, updatedItem, error) = await SyncOperations.Operation.$current.withValue(operation) {
                        await self.performFetchContents(
                            for: itemIdentifier,
                            progress: progress,
                            cancellation: cancellation,
                            actionId: actionId,
                            dbManager: dbManager
                        )
                    }
                    if self.retryIfStopped(operation, completion: { completionHandler(nil, nil, $0) }) { return }
                    completionHandler(localUrl, updatedItem, error)
                } catch {
                    if (error as? NSFileProviderError)?.code == .notAuthenticated {
                        self.insertErrorAction(actionId)
                    } else {
                        self.removeSyncAction(actionId)
                    }
                    completionHandler(nil, nil, cancellation.isCancelled ? CocoaError(.userCancelled) : error)
                }
            }
        }

        return progress
    }

    private func performFetchContents(
        for itemIdentifier: NSFileProviderItemIdentifier,
        progress: Progress,
        cancellation: NetworkOperationCancellation,
        actionId: UUID,
        dbManager: FilesDatabaseManager
    ) async -> (URL?, Item?, Error?) {
        guard !cancellation.isCancelled else {
            removeSyncAction(actionId)
            return (nil, nil, CocoaError(.userCancelled))
        }
        let item: Item
        do {
            item = try await Item.lookupItemAndFindParentIdentifier(
                identifier: itemIdentifier,
                account: dbManager.account,
                remoteInterface: ncKit,
                dbManager: dbManager,
                taskHandler: { cancellation.register(task: $0) },
                isCancelled: { cancellation.isCancelled },
                log: log
            )
        } catch {
            if cancellation.isCancelled {
                removeSyncAction(actionId)
                return (nil, nil, CocoaError(.userCancelled))
            }
            logger.error("Could not look up the item for content fetch.", [.item: itemIdentifier, .error: error])
            insertErrorAction(actionId)
            return (nil, nil, error)
        }
        guard !cancellation.isCancelled else {
            removeSyncAction(actionId)
            return (nil, nil, CocoaError(.userCancelled))
        }

        let result = await item.performFetchContents(domain: domain, progress: progress, dbManager: dbManager, cancellation: cancellation)
        removeSyncAction(actionId)
        return result
    }

    public func createItem(
        basedOn itemTemplate: NSFileProviderItem,
        fields: NSFileProviderItemFields,
        contents url: URL?,
        options _: NSFileProviderCreateItemOptions = [],
        request: NSFileProviderRequest,
        completionHandler: @Sendable @escaping (
            NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?
        ) -> Void
    ) -> Progress {
        guard !blockSync else {
            logger.info("Not creating item because synchronization is blocked.", [.item: itemTemplate.itemIdentifier, .name: itemTemplate.filename])
            completionHandler(itemTemplate, [], false, NSFileProviderError(.serverUnreachable))

            return Progress()
        }

        let actionId = UUID()
        insertSyncAction(actionId)
        logger.debug("Received request to create item.", [.item: itemTemplate.itemIdentifier, .name: itemTemplate.filename])

        guard let ignoredFiles else {
            logger.debug("Not creating item because ignore list not set up yet.", [.item: itemTemplate.itemIdentifier])
            insertErrorAction(actionId)
            completionHandler(itemTemplate, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let progress = Progress()
        let (dbManager, operation): (FilesDatabaseManager, SyncOperations.Operation)
        do {
            (dbManager, operation) = try beginSyncOperation(resumeWrite: { progress.resume() })
        } catch {
            logger.debug("Not creating item because the account is unavailable.", [.item: itemTemplate.itemIdentifier])
            insertErrorAction(actionId)
            completionHandler(itemTemplate, [], false, error)
            return Progress()
        }

        operation.run { [self] in
            let (item, error) = await Item.create(
                basedOn: itemTemplate,
                fields: fields,
                contents: url,
                request: request,
                domain: self.domain,
                account: dbManager.account,
                remoteInterface: ncKit,
                ignoredFiles: ignoredFiles,
                progress: progress,
                dbManager: dbManager,
                appProxy: app,
                log: log
            )

            if retryIfStopped(operation, completion: { completionHandler(itemTemplate, [], false, $0) }) {
                removeSyncAction(actionId)
                return
            }
            if let fileProviderError = error as? NSFileProviderError, fileProviderError.code == .excludedFromSync {
                removeSyncAction(actionId)
            } else {
                completeSyncAction(actionId, error: error)
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

    public func modifyItem(
        _ item: NSFileProviderItem,
        baseVersion: NSFileProviderItemVersion,
        changedFields: NSFileProviderItemFields,
        contents newContents: URL?,
        options: NSFileProviderModifyItemOptions = [],
        request: NSFileProviderRequest,
        completionHandler: @Sendable @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?) -> Void
    ) -> Progress {
        // An item was modified on disk, process the item's modification
        // TODO: Handle finder things like tags, other possible item changed fields
        guard !blockSync else {
            logger.info("Not modifying item because synchronization is blocked.", [.item: item.itemIdentifier])
            completionHandler(item, [], false, NSFileProviderError(.serverUnreachable))

            return Progress()
        }

        let actionId = UUID()
        insertSyncAction(actionId)

        let identifier = item.itemIdentifier
        logger.debug("Received request to modify item.", [.item: item.itemIdentifier])

        guard let ignoredFiles else {
            logger.debug("Not modifying item because ignore list not set up yet.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let progress = Progress()
        let (dbManager, operation): (FilesDatabaseManager, SyncOperations.Operation)
        do {
            (dbManager, operation) = try beginSyncOperation(resumeWrite: { progress.resume() })
        } catch {
            logger.debug("Not modifying item because the account is unavailable.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(item, [], false, error)
            return Progress()
        }

        operation.run { [self] in
            let existingItem: Item
            do {
                existingItem = try await Item.lookupItemAndFindParentIdentifier(
                    identifier: identifier,
                    account: dbManager.account,
                    remoteInterface: ncKit,
                    dbManager: dbManager,
                    lookupRetryError: Item.itemLookupRetryErrorForMutations,
                    taskHandler: { operation.registerReadTask($0) },
                    log: log
                )
            } catch {
                if retryIfStopped(operation, completion: { completionHandler(item, [], false, $0) }) {
                    removeSyncAction(actionId)
                    return
                }
                logger.error("Could not look up the item for modification.", [.item: identifier, .error: error])
                insertErrorAction(actionId)
                completionHandler(item, [], false, error)
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
                dbManager: dbManager,
                appProxy: app
            )

            if retryIfStopped(operation, completion: { completionHandler(item, [], false, $0) }) {
                removeSyncAction(actionId)
                return
            }
            completeSyncAction(actionId, error: error)

            logger.debug("Calling item modification completion handler.", [.item: item.itemIdentifier, .name: item.filename, .error: error])
            completionHandler(modifiedItem ?? item, [], false, error)
        }

        return progress
    }

    public func deleteItem(
        identifier: NSFileProviderItemIdentifier,
        baseVersion _: NSFileProviderItemVersion,
        options _: NSFileProviderDeleteItemOptions = [],
        request _: NSFileProviderRequest,
        completionHandler: @Sendable @escaping (Error?) -> Void
    ) -> Progress {
        guard !blockSync else {
            logger.info("Not deleting item because synchronization is blocked.", [.item: identifier])
            completionHandler(NSFileProviderError(.serverUnreachable))

            return Progress()
        }

        let actionId = UUID()
        insertSyncAction(actionId)

        logger.debug("Received request to delete item.", [.item: identifier])

        guard let ignoredFiles else {
            logger.debug("Not deleting item because ignore list not received.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let (dbManager, operation): (FilesDatabaseManager, SyncOperations.Operation)
        do {
            (dbManager, operation) = try beginSyncOperation()
        } catch {
            logger.debug("Not deleting item because the account is unavailable.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(error)
            return Progress()
        }

        let progress = Progress(totalUnitCount: 1)
        operation.run { [self] in
            let item: Item
            do {
                item = try await Item.lookupItemAndFindParentIdentifier(
                    identifier: identifier,
                    account: dbManager.account,
                    remoteInterface: ncKit,
                    dbManager: dbManager,
                    lookupRetryError: Item.itemLookupRetryErrorForMutations,
                    taskHandler: { operation.registerReadTask($0) },
                    log: log
                )
            } catch {
                if retryIfStopped(operation, completion: completionHandler) {
                    removeSyncAction(actionId)
                    return
                }
                logger.error("Could not look up the item for deletion.", [.item: identifier, .error: error])
                insertErrorAction(actionId)
                completionHandler(error)
                return
            }

            logger.debug("Found item for identifier.", [.item: identifier, .name: item.filename])
            let error = await item.delete(domain: domain, ignoredFiles: ignoredFiles, dbManager: dbManager)

            if retryIfStopped(operation, completion: completionHandler) {
                removeSyncAction(actionId)
                return
            }
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

    public func enumerator(for containerItemIdentifier: NSFileProviderItemIdentifier, request _: NSFileProviderRequest) throws -> NSFileProviderEnumerator {
        logger.debug("System requested enumerator.", [.item: containerItemIdentifier])

        // Refusing here is what stops remote changes arriving: the app discovers them and signals the working set, but the signal carries no data and reaches this extension as a request for an enumerator, so declining it declines both the push and the polling path.
        //
        // It has to be an error rather than an enumerator which reports nothing. An empty but successful enumeration tells the framework that the container is empty, which for a replicated extension is an instruction to delete every child of it from disk. Failing loses nothing; succeeding emptily loses the user's files.
        guard !blockSync else {
            logger.info("Not providing enumerator for item because synchronization is blocked.", [.item: containerItemIdentifier])
            throw NSFileProviderError(.serverUnreachable)
        }

        let (dbManager, operation) = try beginSyncOperation()
        defer { operation.finish() }

        return try Enumerator(
            enumeratedItemIdentifier: containerItemIdentifier,
            remoteInterface: ncKit,
            dbManager: dbManager,
            domain: domain,
            log: log
        )
    }

    public func materializedItemsDidChange(completionHandler: @escaping () -> Void) {
        refreshMaterializedItems(onlyIfPending: false, completionHandler: completionHandler)
    }

    private func refreshDeferredMaterializedItems() {
        refreshMaterializedItems(onlyIfPending: true, completionHandler: {})
    }

    private func refreshMaterializedItems(onlyIfPending: Bool, completionHandler: @escaping () -> Void) {
        guard enumerateMaterializedItems != nil || manager != nil else {
            logger.debug("Could not get materialized items enumerator.")
            completionHandler()
            return
        }
        let work = setupLock.withLock { () -> (FilesDatabaseManager, SyncOperations.Operation)? in
            let refreshWasStopped = materializedItemsRefresh?.isStopped == true
            guard !onlyIfPending || materializedItemsRefreshPending || refreshWasStopped else { return nil }
            guard materializedItemsRefresh == nil || refreshWasStopped,
                  let dbManager = publishedDatabaseManager,
                  let operation = dbManager.syncOperations.begin()
            else {
                // One fresh snapshot covers all callbacks deferred during setup or an earlier snapshot.
                materializedItemsRefreshPending = true
                return nil
            }
            materializedItemsRefresh = operation
            materializedItemsRefreshPending = false
            return (dbManager, operation)
        }
        guard let (dbManager, operation) = work else {
            completionHandler()
            return
        }
        let observer = MaterializedEnumerationObserver(
            account: dbManager.account,
            dbManager: dbManager,
            log: log
        ) { [weak self] materialized, evicted in
            // Covers eviction (files going dataless) and out-of-band
            // materialization the observer discovers itself. Downloads via
            // `Item.fetchContents` are handled there instead, because they update
            // the database before this enumeration runs (#10085).
            let changed = materialized.union(evicted)
            if let self {
                if !operation.isStopped, let manager, !changed.isEmpty {
                    refreshRemoveDownloadVisibility(
                        forAncestorsOfFileOcIds: Set(changed.map(\.rawValue)),
                        domain: domain.identifier,
                        manager: manager,
                        dbManager: dbManager,
                        logger: logger
                    )
                }
                setupLock.withLock {
                    if materializedItemsRefresh === operation {
                        materializedItemsRefresh = nil
                        if operation.isStopped { materializedItemsRefreshPending = true }
                    }
                }
            }
            operation.finish()
            completionHandler()
            self?.refreshDeferredMaterializedItems()
        }
        observer.syncOperation = operation
        let startingPage = NSFileProviderPage(NSFileProviderPage.initialPageSortedByName as Data)

        if let enumerateMaterializedItems {
            enumerateMaterializedItems(observer, startingPage)
        } else {
            manager?.enumeratorForMaterializedItems().enumerateItems(for: observer, startingAt: startingPage)
        }
    }

    // MARK: - Helper functions

    func signalEnumerator(completionHandler: @Sendable @escaping (_ error: Error?) -> Void) {
        guard let manager else {
            logger.error("Cannot get file provider manager for domain. Cannot signal enumerator.")
            return
        }

        manager.signalEnumerator(for: .workingSet, completionHandler: completionHandler)
    }

    private func signalEnumeratorAfterAccountSetup() {
        guard let manager else {
            logger.error("Could not get manager for domain. Cannot signal enumerator after account setup.")
            return
        }

        guard dbManager != nil else { return }

        manager.signalErrorResolved(NSFileProviderError(.notAuthenticated)) { error in
            if error != nil {
                self.logger.error("Cannot resolve .notAuthenticated error.", [.error: error?.localizedDescription])
            }
        }

        signalFileProviderToRetryUnreachableRequests()

        logger.debug("Signalling enumerators.")
        notifyChange()

        // Also nudge the root container so its enumerator's `enumerateChanges(for:from:)` is
        // invoked shortly after the extension starts. Best-effort: when no enumerator is
        // active for the root container at signal time, the framework debounces the signal
        // and the call is effectively a no-op. The reliable refresh of cached
        // `NSFileProviderItem` snapshots after an extension version bump happens via
        // ``refreshFrameworkCacheIfExtensionVersionChanged`` below; this signal just shortens
        // the path on launches where Finder is already actively viewing the root.
        manager.signalEnumerator(for: .rootContainer) { error in
            if error != nil {
                self.logger.error("Failed to signal root container enumerator after account setup.", [.error: error?.localizedDescription])
            }
        }

        refreshFrameworkCacheIfExtensionVersionChanged()
    }

    private func signalFileProviderToRetryUnreachableRequests() {
        // Unpausing sync will release these requests if sync is paused.
        guard !blockSync, let manager else { return }
        manager.signalErrorResolved(NSFileProviderError(.serverUnreachable)) { error in
            if let error {
                self.logger.error("Could not tell File Provider to retry requests held during account setup.", [.error: error])
            }
        }
    }

    ///
    /// On the first launch after the extension bundle's `CFBundleShortVersionString` changes, walk every non-deleted item in the database and call `NSFileProviderManager.requestModification(of: [.lastUsedDate], forItemWithIdentifier:)` for each.
    ///
    /// Each `requestModification` schedules a `modifyItem(…)` call from the framework into this extension. Our `modifyItem` builds a fresh ``Item`` from the database row and returns it via the completion handler — and the fresh `Item`'s ``Item/itemVersion`` carries a `metadataVersion` that embeds the current extension version (see ``Item/itemVersion``). The framework compares that against its cached `metadataVersion` (which was derived under the previous extension version), detects the mismatch, and writes the fresh snapshot — including the freshly computed `userInfo` keys, `contentPolicy`, `fileSystemFlags`, etc. — into its cache.
    ///
    /// Without this iteration, only items in the working set get their cached snapshots refreshed after a version bump (via the version-tagged sync anchor in ``Enumerator``). Placeholder children of containers Finder isn't actively enumerating — the bulk of items in a fresh sync — would otherwise keep their pre-upgrade snapshots until the user actively interacts with them. The same `requestModification(of: [.lastUsedDate], …)` nudge is used by ``Item/signalKeepDownloaded`` for individual keep-downloaded toggles.
    ///
    /// The current bundle version is persisted to ``FileProviderDomainDefaults/lastSeenExtensionVersion`` only after the iteration has finished issuing every nudge, so an interrupted launch redoes the work on the next start rather than skipping items.
    ///
    /// See nextcloud/desktop#10065.
    ///
    private func refreshFrameworkCacheIfExtensionVersionChanged() {
        guard let manager else {
            logger.error("Cannot refresh framework cache because the file provider manager is not available.")
            return
        }

        guard let dbManager else {
            logger.error("Cannot refresh framework cache because the account is not set up.")
            return
        }

        guard !dbManager.wasInvalidatedByDAVAccountChange else {
            logger.error("Cannot refresh framework cache because the database manager is not available.")
            return
        }

        let bundle = Bundle(for: type(of: self))

        guard let currentVersion = bundle.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String, currentVersion.isEmpty == false else {
            logger.error("Cannot refresh framework cache because the extension bundle has no CFBundleShortVersionString.")
            return
        }

        let lastSeenVersion = config.lastSeenExtensionVersion

        guard currentVersion != lastSeenVersion else {
            logger.debug("Extension version \"\(currentVersion)\" unchanged since last launch. Skipping framework cache refresh.")
            return
        }

        let items = dbManager
            .itemMetadatas(account: dbManager.account.ncKitAccount)
            .filter { metadata in
                !metadata.deleted
                    && metadata.ocId != NSFileProviderItemIdentifier.rootContainer.rawValue
                    && metadata.ocId != NSFileProviderItemIdentifier.trashContainer.rawValue
            }

        logger.info("Extension version changed from \"\(lastSeenVersion ?? "<nil>")\" to \"\(currentVersion)\". Requesting metadata refresh for \(items.count) item(s).")

        // Fire-and-forget: each `requestModification` returns quickly after scheduling the
        // framework's modifyItem call. Sequential awaits keep the request-queue throughput
        // predictable and avoid spawning one Swift `Task` per database row (which for large
        // synced sets would be wasteful even if the system-level queue would coalesce them).
        Task { [weak self] in
            guard let self else {
                return
            }

            for metadata in items {
                let identifier = NSFileProviderItemIdentifier(metadata.ocId)

                do {
                    try await manager.requestModification(of: [.lastUsedDate], forItemWithIdentifier: identifier)
                } catch {
                    logger.error("Failed to request modification on item for framework cache refresh after extension version bump.", [.item: identifier, .error: error.localizedDescription])
                }
            }

            // Persist the new version once every nudge has been issued — even if some of
            // them errored. The next launch under the same version will skip this work; the
            // small subset of items that errored will refresh through normal user-interaction
            // paths (item(for:), explicit enumeration) once Finder touches them, because the
            // `metadataVersion` comparison still detects the mismatch on those paths.
            config.lastSeenExtensionVersion = currentVersion
            logger.info("Framework cache refresh completed after extension version bump.", [.account: dbManager.account])
        }
    }

    ///
    /// Concurrent invocations of this method are serialized: at most one `performSetup(…)` runs
    /// at a time. Duplicate invocations — by value equality on ``Account`` — received while a
    /// setup is already in flight (tracked via ``pendingAccount``) or already completed (tracked
    /// via the published database manager) are dropped silently. This prevents concurrent XPC
    /// callers from each opening a NextcloudKit session and database manager for the same account.
    ///
    /// - Parameters:
    ///     - completionHandler: An optional completion handler which will be provided an error, if any occurred. Omitting this completion handler is fine, but you won't get notified of errors.
    ///
    @objc func setupDomainAccount(
        user: String,
        userId: String,
        serverUrl: String,
        password: String,
        userAgent: String = "Nextcloud-macOS/FileProviderExt",
        completionHandler: (@Sendable (NSError?) -> Void)? = nil
    ) {
        let account = Account(user: user, id: userId, serverUrl: serverUrl, password: password)

        logger.info("Setting up domain account for user: \(user), userId: \(userId), serverUrl: \(serverUrl), password: \(password.isEmpty ? "<empty>" : "<not-empty>"), ncKitAccount: \(account.ncKitAccount)")

        guard password.isEmpty == false else {
            logger.info("Cancelling domain account setup because \"password\" is an empty string.")
            completionHandler?(NSError(.missingAccountInformation))
            return
        }

        guard serverUrl.isEmpty == false else {
            logger.info("Cancelling domain account setup because \"serverUrl\" is an empty string.")
            completionHandler?(NSError(.missingAccountInformation))
            return
        }

        guard user.isEmpty == false else {
            logger.info("Cancelling domain account setup because \"user\" is an empty string.")
            completionHandler?(NSError(.missingAccountInformation))
            return
        }

        guard userId.isEmpty == false else {
            logger.info("Cancelling domain account setup because \"userId\" is an empty string.")
            completionHandler?(NSError(.missingAccountInformation))
            return
        }

        setupLock.lock()

        let accountIsAlreadySetUp = pendingAccount == nil
            && account == publishedDatabaseManager?.account
            && publishedDatabaseManager?.syncOperations.isAcceptingWork == true
        if accountIsAlreadySetUp || account == pendingAccount {
            setupLock.unlock()
            logger.info("Cancelling domain account setup because of receiving the same account information repeatedly.")
            completionHandler?(nil)
            return
        }

        pendingAccount = account
        let previous = setupChain
        let generation = setupGeneration

        let next = Task { [weak self] in
            _ = await previous.value

            guard let self else {
                return
            }

            await performSetup(account: account, generation: generation, userAgent: userAgent) { error in
                self.clearPendingAccount(matching: account, generation: generation)
                completionHandler?(error)
            }
        }

        setupChain = next
        setupLock.unlock()
    }

    /// Allow another setup for this account before notifying the caller of the result.
    private func clearPendingAccount(matching account: Account, generation: Int) {
        setupLock.withLock {
            if generation == setupGeneration, pendingAccount == account {
                pendingAccount = nil
            }
        }
    }

    /// Save the password in the Keychain. With `awaitingDatabaseSetup: true`, save the incoming
    /// account separately for Finder retries so the migration from absolute WebDAV parent URLs
    /// to paths such as `files:/Team` can still read the previous account.
    /// With `false`, replace the domain's configured account and clear the retry details.
    func storeAccount(_ account: Account, generation: Int, awaitingDatabaseSetup: Bool = false) -> Bool {
        setupLock.lock()
        defer { setupLock.unlock() }

        guard generation == setupGeneration else {
            return false
        }

        keychain.savePassword(account.password, for: account.username)
        if awaitingDatabaseSetup {
            config.accountForSetupRetry = account.davAccountIdentity
        } else {
            config.recordAccountAfterDatabaseSetup(account)
        }
        return true
    }

    /// Prefer the incoming account so Finder retries with the same server and credentials after a setup failure.
    func storedAccountForAuthentication() -> Account? {
        setupLock.withLock {
            guard let identity = config.accountForSetupRetry ?? config.configuredDAVAccountIdentity,
                  let password = keychain.getPassword(for: identity.username)
            else { return nil }

            return Account(davAccountIdentity: identity, password: password)
        }
    }

    ///
    /// Make the database manager and its account available to requests.
    /// A setup superseded by ``removeAccount()`` cannot publish its manager.
    ///
    func publishDatabaseManager(
        _ databaseManager: FilesDatabaseManager,
        generation: Int
    ) -> Bool {
        let published = setupLock.withLock {
            guard generation == setupGeneration else { return false }
            publishedDatabaseManager = databaseManager
            return true
        }
        if published {
            refreshDeferredMaterializedItems()
        }
        return published
    }

    ///
    /// Forget the account and delete its stored password, superseding every setup queued or running until now.
    ///
    /// - Returns: `true` if no password of the domain is left in the keychain.
    ///
    @discardableResult
    func removeAccount(drainSyncOperations: Bool = true) -> Bool {
        setupLock.lock()
        defer { setupLock.unlock() }

        setupGeneration += 1
        pendingAccount = nil
        config.accountForSetupRetry = nil
        if let previousManager = publishedDatabaseManager {
            if drainSyncOperations {
                previousManager.syncOperations.stopAcceptingWork()
                let previousSetup = setupChain
                // Redirects remove the account before configuring the new URL. Keep the old
                // manager writable until its requests finish, and make the next setup wait for them.
                setupChain = Task {
                    await previousManager.syncOperations.waitForWrites()
                    previousManager.invalidateForDAVAccountChange()
                    await previousSetup.value
                }
            } else {
                previousManager.invalidateForDAVAccountChange()
            }
        }
        publishedDatabaseManager = nil

        return keychain.deletePassword()
    }

    /// Keep the current session available while checking replacement credentials.
    private func authenticateDAVAccount(
        _ account: Account,
        generation: Int,
        userAgent: String,
        validatingReplacement: Bool
    ) async -> AuthenticationAttemptResultState? {
        let sessionKey: String
        if validatingReplacement {
            sessionKey = makeDAVValidationSession(ncKit, account, userAgent)
        } else {
            ncKit.appendSession(account: account, userAgent: userAgent)
            sessionKey = account.ncKitAccount
        }
        defer {
            if validatingReplacement { ncKit.removeDAVValidationSession(sessionKey) }
        }

        let options = NKRequestOptions(checkInterceptor: false)
        var result = await ncKit.validateDAVCredentials(account: account, sessionKey: sessionKey, options: options)
        for delay in authenticationRetryDelays {
            guard result == .connectionError else { break }
            logger.info("Retrying the connection check after a delay.")
            try? await Task.sleep(nanoseconds: delay)
            guard setupLock.withLock({ generation == setupGeneration }) else { return nil }
            result = await ncKit.validateDAVCredentials(account: account, sessionKey: sessionKey, options: options)
        }
        return result
    }

    /// Cancel reads and let sent writes finish before updating and publishing the replacement.
    private func replaceDAVAccount(
        _ account: Account,
        previousManager: FilesDatabaseManager,
        generation: Int,
        userAgent: String
    ) async throws -> FilesDatabaseManager? {
        previousManager.syncOperations.stopAcceptingWork()
        await previousManager.syncOperations.waitForWrites()
        do {
            return try setupLock.withLock {
                guard generation == setupGeneration,
                      publishedDatabaseManager === previousManager
                else { return nil }

                let manager = try previousManager.replacingDAVAccount(with: account)
                ncKit.appendSession(account: account, userAgent: userAgent)
                keychain.savePassword(account.password, for: account.username)
                config.recordAccountAfterDatabaseSetup(account)
                previousManager.invalidateForDAVAccountChange()
                publishedDatabaseManager = manager
                return manager
            }
        } catch {
            if previousManager.isForCurrentDAVAccount() {
                let resumed = setupLock.withLock {
                    guard generation == setupGeneration,
                          publishedDatabaseManager === previousManager
                    else { return false }
                    return previousManager.syncOperations.resumeAcceptingWork()
                }
                if resumed {
                    signalFileProviderToRetryUnreachableRequests()
                    refreshDeferredMaterializedItems()
                }
            }
            throw error
        }
    }

    ///
    /// Runs the actual account setup work for one ``Account``.
    ///
    /// Must only be invoked from the ``setupChain`` serialization queue established by
    /// ``setupDomainAccount(user:userId:serverUrl:password:userAgent:completionHandler:)``.
    ///
    private func performSetup(
        account: Account,
        generation: Int,
        userAgent: String,
        completionHandler: (@Sendable (NSError?) -> Void)?
    ) async {
        let previousDatabaseManager = dbManager
        guard setupLock.withLock({ generation == setupGeneration }),
              previousDatabaseManager != nil || storeAccount(account, generation: generation, awaitingDatabaseSetup: true)
        else {
            completionHandler?(NSError(.missingAccountInformation))
            return
        }

        guard let authAttemptState = await authenticateDAVAccount(
            account,
            generation: generation,
            userAgent: userAgent,
            validatingReplacement: previousDatabaseManager != nil
        ), setupLock.withLock({ generation == setupGeneration }) else {
            completionHandler?(NSError(.missingAccountInformation))
            return
        }
        switch authAttemptState {
            case .authenticationError:
                logger.error("Authentication failed due to bad credentials.")
                completionHandler?(NSError(.invalidCredentials))
                return
            case .connectionError:
                logger.error("Authentication failed due to a connection error.")
                completionHandler?(NSError(.connection))
                return
            case .success:
                logger.info("Successfully authenticated.")
        }

        let databaseManager: FilesDatabaseManager
        if let previousDatabaseManager {
            do {
                guard let replacement = try await replaceDAVAccount(
                    account,
                    previousManager: previousDatabaseManager,
                    generation: generation,
                    userAgent: userAgent
                ) else {
                    completionHandler?(NSError(.missingAccountInformation))
                    return
                }
                databaseManager = replacement
            } catch {
                logger.fault("Could not update the metadata database for the incoming account.", [.error: error])
                completionHandler?(NSError(.databaseUnavailable))
                return
            }
        } else {
            // Cold setup can import Realm or recover a corrupt database.
            do {
                databaseManager = try FilesDatabaseManager(
                    account: account,
                    databaseDirectory: databaseDirectory,
                    fileProviderDomainIdentifier: domain.identifier,
                    log: log,
                    pendingMaterializations: pendingMaterializations
                )
            } catch {
                logger.fault("Could not open the metadata database for the incoming account.", [.error: error])
                completionHandler?(NSError(.databaseUnavailable))
                return
            }
            guard storeAccount(account, generation: generation) else {
                logger.info("Cancelling domain account setup because the account was removed meanwhile.")
                completionHandler?(NSError(.missingAccountInformation))
                return
            }
        }

        await MainActor.run {
            let published = if previousDatabaseManager == nil {
                publishDatabaseManager(databaseManager, generation: generation)
            } else {
                setupLock.withLock {
                    generation == setupGeneration && publishedDatabaseManager === databaseManager
                }
            }
            guard published else {
                logger.info("Not publishing the authenticated account because it was removed meanwhile.")
                completionHandler?(NSError(.missingAccountInformation))
                return
            }
            if previousDatabaseManager != nil { refreshDeferredMaterializedItems() }

            // TODO: Initial file creation does not persist item metadata until the upload succeeds.
            // If the extension restarts while that upload is still in progress, startup cleanup
            // cannot distinguish its chunks from an abandoned upload and may remove them.
            cleanupAbandonedChunkUploads(
                usingRemoteInterface: ncKit,
                dbManager: databaseManager,
                logger: logger
            )
            ncKit.setup(groupIdentifier: Bundle.main.bundleIdentifier)
            signalAccountReady()
            completionHandler?(nil)
            signalEnumeratorAfterAccountSetup()
        }
    }

    ///
    /// Wait up to `timeoutNanoseconds` for setup to publish the domain's database manager.
    ///
    /// - Throws: `NSFileProviderError(.notAuthenticated)` when setup publishes no manager within
    ///   `timeoutNanoseconds`, or `CancellationError` when the waiting task is cancelled.
    ///
    func awaitDatabaseManager(
        timeoutNanoseconds: UInt64 = 10_000_000_000,
        onWaiting: @Sendable () -> Void = {}
    ) async throws -> FilesDatabaseManager {
        try Task.checkCancellation()
        if let dbManager {
            return dbManager
        }

        let token = UUID()
        var timeoutTask: Task<Void, Never>?
        defer { timeoutTask?.cancel() }

        await withTaskCancellationHandler {
            await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
                accountReadyLock.lock()
                accountReadyWaiters[token] = continuation
                accountReadyLock.unlock()

                // Setup may have published the manager between the check above and the enqueue.
                if dbManager != nil || Task.isCancelled {
                    resumeAccountWaiter(token)
                    return
                }

                timeoutTask = Task { [weak self] in
                    do {
                        try await Task.sleep(nanoseconds: timeoutNanoseconds)
                    } catch {
                        return
                    }
                    self?.resumeAccountWaiter(token)
                }
                onWaiting()
            }
        } onCancel: {
            self.resumeAccountWaiter(token)
        }
        try Task.checkCancellation()

        // Read after resuming so the request can use a manager that setup published
        // just as the wait timed out.
        guard let dbManager else {
            throw NSFileProviderError(.notAuthenticated)
        }

        return dbManager
    }

    ///
    /// Resume one parked waiter, if it has not been resumed already.
    ///
    private func resumeAccountWaiter(_ token: UUID) {
        accountReadyLock.lock()
        let continuation = accountReadyWaiters.removeValue(forKey: token)
        accountReadyLock.unlock()
        continuation?.resume()
    }

    ///
    /// Release the requests waiting in ``awaitDatabaseManager(timeoutNanoseconds:onWaiting:)``.
    ///
    func signalAccountReady() {
        accountReadyLock.lock()
        let waiters = accountReadyWaiters
        accountReadyWaiters.removeAll()
        accountReadyLock.unlock()

        if !waiters.isEmpty {
            logger.debug("Account is set up; releasing \(waiters.count) waiting request(s).")
        }

        for continuation in waiters.values {
            continuation.resume()
        }
    }

    func updatedSyncStateReporting(oldActions: Set<UUID>) {
        actionsLock.lock()

        guard oldActions.isEmpty != syncActions.isEmpty else {
            logger.debug("Cancelling synchronization state report due to lack of state change.")
            actionsLock.unlock()
            return
        }

        var argument: String?

        if oldActions.isEmpty, !syncActions.isEmpty {
            argument = "SYNC_STARTED"
        } else if !oldActions.isEmpty, syncActions.isEmpty {
            argument = errorActions.isEmpty ? "SYNC_FINISHED" : "SYNC_FAILED"
            errorActions = []
        }

        actionsLock.unlock()

        guard let argument else {
            logger.error("State argument is nil.")
            return
        }

        logger.debug("Reporting synchronization state.", [.name: argument])

        app?.reportSyncStatus(argument, forDomainIdentifier: domain.identifier.rawValue)
    }

    /// Reports the extension's current synchronization state to the app unconditionally.
    ///
    /// Unlike ``updatedSyncStateReporting(oldActions:)``, which is edge-triggered and deliberately
    /// bails when sync activity has not changed, this always emits a report. The main app opens an
    /// XPC connection to the extension on launch; at that point it needs the current resting state
    /// — typically idle, i.e. "all good" — otherwise it falls back to the misleading
    /// "Some files could not be synced!" message. See
    /// https://github.com/nextcloud/desktop/issues/10053.
    func reportCurrentSyncState() {
        actionsLock.lock()
        let syncing = !syncActions.isEmpty
        let failed = !errorActions.isEmpty
        actionsLock.unlock()

        let argument = if syncing {
            "SYNC_STARTED"
        } else if failed {
            "SYNC_FAILED"
        } else {
            "SYNC_FINISHED"
        }

        logger.debug("Reporting current synchronization state on request.", [.name: argument])

        app?.reportSyncStatus(argument, forDomainIdentifier: domain.identifier.rawValue)
    }
}

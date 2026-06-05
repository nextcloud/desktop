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

    let keychain: Keychain
    let log: any FileProviderLogging
    let logger: FileProviderLogger

    ///
    /// The file provider manager for the domain managed by this extension implementation.
    ///
    let manager: NSFileProviderManager?

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

    var ncAccount: Account?
    var dbManager: FilesDatabaseManager?
    var ignoredFiles: IgnoredFilesMatcher?
    lazy var ncKitBackground = NKBackground(nkCommonInstance: ncKit.nkCommonInstance)

    var syncActions = Set<UUID>()
    var errorActions = Set<UUID>()
    var actionsLock = NSLock()

    // Serialization state for `setupDomainAccount(…)`. See the method for details.
    private let setupLock = NSLock()
    private var pendingAccount: Account?
    private var setupChain: Task<Void, Never> = Task {}

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

    public required init(domain: NSFileProviderDomain) {
        // The containing application must create a domain using
        // `NSFileProviderManager.add(_:, completionHandler:)`. The system will then launch the
        // application extension process, call `FileProviderExtension.init(domain:)` to instantiate
        // the extension for that domain, and call methods on the instance.
        self.domain = domain
        manager = NSFileProviderManager(for: domain)

        // Set up logging.
        log = FileProviderLog(fileProviderDomainIdentifier: domain.identifier)
        logger = FileProviderLogger(category: "FileProviderExtension", log: log)
        logger.debug("Initializing with domain identifier.", [.domain: domain.identifier.rawValue])

        // Set up NextcloudKit.
        ncKit = NextcloudKit.shared

        #if DEBUG
            NKLogFileManager.configure(logLevel: .verbose)
        #else
            NKLogFileManager.configure(logLevel: .normal)
        #endif

        logger.info("NextcloudKit logging configured.", [.url: NKLogFileManager.shared.currentLogFileURL()])
        keychain = Keychain(log: log)
        super.init()
    }

    public func invalidate() {
        logger.debug("File provider extension process is being invalidated.")
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

    // MARK: - NSFileProviderReplicatedExtension protocol methods

    public func item(for identifier: NSFileProviderItemIdentifier, request _: NSFileProviderRequest, completionHandler: @Sendable @escaping (NSFileProviderItem?, Error?) -> Void) -> Progress {
        logger.debug("Received request for item.", [.item: identifier])

        guard let ncAccount else {
            logger.debug("Not fetching item because account not set up yet.", [.item: identifier])
            completionHandler(nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            logger.debug("Not fetching item because database is unavailable.", [.item: identifier])
            completionHandler(nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        let progress = Progress(totalUnitCount: 1)

        Task {
            if let item = await Item.storedItem(identifier: identifier, account: ncAccount, remoteInterface: ncKit, dbManager: dbManager, log: log), item.metadata.deleted == false {
                progress.completedUnitCount = 1
                completionHandler(item, nil)
            } else {
                completionHandler(nil, NSFileProviderError(.noSuchItem))
            }
        }

        return progress
    }

    public func fetchContents(for itemIdentifier: NSFileProviderItemIdentifier, version requestedVersion: NSFileProviderItemVersion?, request _: NSFileProviderRequest, completionHandler: @Sendable @escaping (URL?, NSFileProviderItem?, Error?) -> Void) -> Progress {
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

        guard let ncAccount else {
            logger.debug("Not fetching contents for item because account not set up yet.", [.item: itemIdentifier])
            insertErrorAction(actionId)
            completionHandler(nil, nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            logger.debug("Not fetching contents for item because database is unavailable.", [.item: itemIdentifier])
            completionHandler(nil, nil, NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        let progress = Progress()

        Task {
            guard let item = await Item.storedItem(identifier: itemIdentifier, account: ncAccount, remoteInterface: ncKit, dbManager: dbManager, log: log) else {
                logger.error("Not fetching contents for item because item was not found.", [.item: itemIdentifier])
                completionHandler(nil, nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier))
                insertErrorAction(actionId)
                return
            }

            let (localUrl, updatedItem, error) = await item.fetchContents(domain: self.domain, progress: progress, dbManager: dbManager)
            removeSyncAction(actionId)
            completionHandler(localUrl, updatedItem, error)
        }

        return progress
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
        let actionId = UUID()
        insertSyncAction(actionId)
        logger.debug("Received request to create item.", [.item: itemTemplate.itemIdentifier, .name: itemTemplate.filename])

        guard let ncAccount else {
            logger.debug("Not creating item because account is not set up yet.", [.item: itemTemplate.itemIdentifier])
            insertErrorAction(actionId)
            completionHandler(itemTemplate, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let ignoredFiles else {
            logger.debug("Not creating item because ignore list not set up yet.", [.item: itemTemplate.itemIdentifier])
            insertErrorAction(actionId)
            completionHandler(itemTemplate, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            logger.debug("Not creating item because database is unavailable.", [.item: itemTemplate.itemIdentifier])
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
                appProxy: app,
                log: log
            )

            if error == nil {
                removeSyncAction(actionId)
            } else {
                if let fileProviderError = error as? NSFileProviderError, fileProviderError.code == .excludedFromSync {
                    removeSyncAction(actionId)
                } else {
                    insertErrorAction(actionId)
                    signalEnumerator(completionHandler: { _ in })
                }
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
        let actionId = UUID()
        insertSyncAction(actionId)

        let identifier = item.itemIdentifier
        logger.debug("Received request to modify item.", [.item: item.itemIdentifier])

        guard let ncAccount else {
            logger.debug("Not modifying item because account not set up yet.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let ignoredFiles else {
            logger.debug("Not modifying item because ignore list not set up yet.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            logger.debug("Not modifying item because the database is unavailable.")
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
                dbManager: dbManager,
                appProxy: app
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

    public func deleteItem(
        identifier: NSFileProviderItemIdentifier,
        baseVersion _: NSFileProviderItemVersion,
        options _: NSFileProviderDeleteItemOptions = [],
        request _: NSFileProviderRequest,
        completionHandler: @Sendable @escaping (Error?) -> Void
    ) -> Progress {
        let actionId = UUID()
        insertSyncAction(actionId)

        logger.debug("Received request to delete item.", [.item: identifier])

        guard let ncAccount else {
            logger.debug("Not deleting item because account is not set up yet.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let ignoredFiles else {
            logger.debug("Not deleting item because ignore list not received.", [.item: identifier])
            insertErrorAction(actionId)
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            logger.debug("Not deleting item because database unavailable.", [.item: identifier])
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

    public func enumerator(for containerItemIdentifier: NSFileProviderItemIdentifier, request _: NSFileProviderRequest) throws -> NSFileProviderEnumerator {
        logger.debug("System requested enumerator.", [.item: containerItemIdentifier])

        guard let ncAccount else {
            logger.debug("Not providing enumerator for item because account is not set up yet.", [.item: containerItemIdentifier])
            throw NSFileProviderError(.notAuthenticated)
        }

        guard let dbManager else {
            logger.debug("Not providing enumerator for item because database is unavailable.", [.item: containerItemIdentifier])
            throw NSFileProviderError(.cannotSynchronize)
        }

        return try Enumerator(
            enumeratedItemIdentifier: containerItemIdentifier,
            account: ncAccount,
            remoteInterface: ncKit,
            dbManager: dbManager,
            domain: domain,
            log: log
        )
    }

    public func materializedItemsDidChange(completionHandler: @escaping () -> Void) {
        guard let ncAccount else {
            logger.debug("Not purging stale local file metadatas because account not set up.")
            completionHandler()
            return
        }

        guard let dbManager else {
            logger.debug("Not purging stale local file metadatas because database is not available.")
            completionHandler()
            return
        }

        guard let manager else {
            logger.debug("Could not get file provider manager.")
            completionHandler()
            return
        }

        let materialisedEnumerator = manager.enumeratorForMaterializedItems()
        let materialisedObserver = MaterializedEnumerationObserver(account: ncAccount, dbManager: dbManager, log: log) { _, _ in
            completionHandler()
        }
        let startingPage = NSFileProviderPage(NSFileProviderPage.initialPageSortedByName as Data)

        materialisedEnumerator.enumerateItems(for: materialisedObserver, startingAt: startingPage)
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

        assert(ncAccount != nil)

        manager.signalErrorResolved(NSFileProviderError(.notAuthenticated)) { error in
            if error != nil {
                self.logger.error("Cannot resolve .notAuthenticated error.", [.error: error?.localizedDescription])
            }
        }

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
            logger.error("Cannot refresh framework cache because the database manager is not available.")
            return
        }

        guard let ncAccount else {
            logger.error("Cannot refresh framework cache because the account is not set up.")
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
            .itemMetadatas(account: ncAccount.ncKitAccount)
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
            logger.info("Framework cache refresh completed after extension version bump.", [.account: ncAccount])
        }
    }

    ///
    /// Concurrent invocations of this method are serialized: at most one `performSetup(…)` runs
    /// at a time. Duplicate invocations — by value equality on ``Account`` — received while a
    /// setup is already in flight (tracked via ``pendingAccount``) or already completed (tracked
    /// via ``ncAccount``) are dropped silently. This prevents concurrent XPC callers from each
    /// opening their own NextcloudKit session and spinning up their own `FilesDatabaseManager`
    /// (and hence their own Realm) for the same credentials.
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

        if account == ncAccount || account == pendingAccount {
            setupLock.unlock()
            logger.info("Cancelling domain account setup because of receiving the same account information repeatedly.")
            completionHandler?(nil)
            return
        }

        pendingAccount = account
        let previous = setupChain

        let next = Task { [weak self] in
            _ = await previous.value

            guard let self else {
                return
            }

            await performSetup(account: account, userAgent: userAgent, completionHandler: completionHandler)
            clearPendingAccount(matching: account)
        }

        setupChain = next
        setupLock.unlock()
    }

    ///
    /// Synchronous cleanup helper used by the ``setupChain`` continuation so the lock acquisition
    /// stays out of the surrounding asynchronous context. `NSLock.lock()` / `unlock()` are
    /// unavailable from async functions in Swift 6 because awaiting across them can deadlock the
    /// cooperative thread pool; doing the lock dance inside this synchronous method sidesteps the
    /// diagnostic without changing the semantics.
    ///
    private func clearPendingAccount(matching account: Account) {
        setupLock.lock()

        if pendingAccount == account {
            pendingAccount = nil
        }

        setupLock.unlock()
    }

    ///
    /// Runs the actual account setup work for one ``Account``.
    ///
    /// Must only be invoked from the ``setupChain`` serialization queue established by
    /// ``setupDomainAccount(user:userId:serverUrl:password:userAgent:completionHandler:)``.
    ///
    private func performSetup(
        account: Account,
        userAgent: String,
        completionHandler: (@Sendable (NSError?) -> Void)?
    ) async {
        // Store account information independently from the main app for later access.
        config.serverUrl = account.serverUrl
        config.user = account.username
        config.userId = account.id
        keychain.savePassword(account.password, for: account.username, on: account.serverUrl)
        NextcloudKit.clearAccountErrorState(for: account.ncKitAccount)

        ncKit.appendSession(
            account: account.ncKitAccount,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id,
            password: account.password,
            userAgent: userAgent,
            groupIdentifier: ""
        )

        var authAttemptState = AuthenticationAttemptResultState.connectionError // default

        // Retry a few times if we have a connection issue
        let options = NKRequestOptions(checkInterceptor: false)

        for authTimeout in AuthenticationTimeouts {
            authAttemptState = await ncKit.tryAuthenticationAttempt(account: account, options: options)

            guard authAttemptState == .connectionError else {
                break
            }

            logger.info("Authentication try timed out. Trying again soon.")
            try? await Task.sleep(nanoseconds: authTimeout)
        }

        switch authAttemptState {
            case .authenticationError:
                logger.error("Authentication failed due to bad credentials.")
                completionHandler?(NSError(.invalidCredentials))
                return
            case .connectionError:
                // Despite multiple connection attempts we are still getting connection issues.
                // Connection error should be provided
                logger.error("Authentication failed due to a connection error.")
                completionHandler?(NSError(.connection))
                return
            case .success:
                logger.info("Successfully authenticated.")
        }

        await MainActor.run {
            ncAccount = account
            dbManager = FilesDatabaseManager(account: account, fileProviderDomainIdentifier: domain.identifier, log: log)

            ncKit.setup(groupIdentifier: Bundle.main.bundleIdentifier!)
            completionHandler?(nil)
            signalEnumeratorAfterAccountSetup()
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
}

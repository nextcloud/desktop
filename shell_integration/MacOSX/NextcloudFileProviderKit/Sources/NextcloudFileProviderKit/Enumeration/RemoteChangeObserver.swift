//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
@preconcurrency import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit

public let NotifyPushAuthenticatedNotificationName = Notification.Name("NotifyPushAuthenticated")

public final class RemoteChangeObserver: NSObject, @unchecked Sendable {
    // @unchecked Sendable is used because 'account' is mutable, but mutation is controlled and safe in this context.
    public let remoteInterface: RemoteInterface
    public let changeNotificationInterface: ChangeNotificationInterface
    public let domain: NSFileProviderDomain?
    public let dbManager: FilesDatabaseManager
    public var account: Account
    public var accountId: String { account.ncKitAccount }

    public var webSocketPingIntervalNanoseconds: UInt64 = 3 * 1_000_000_000
    public let webSocketReconfigureIntervalNanoseconds: UInt64 = 1 * 1_000_000_000
    public let webSocketPingFailLimit = 8
    public let webSocketAuthenticationFailLimit = 3

    public var webSocketTaskActive: Bool {
        webSocketTask != nil
    }

    private let logger: FileProviderLogger

    private var workingSetCheckOngoing = false
    private var invalidated = false

    private var webSocketUrlSession: URLSession?
    private var webSocketTask: URLSessionWebSocketTask?
    private var webSocketOperationQueue = OperationQueue()
    private var webSocketPingTask: Task<Void, Never>?
    private(set) var webSocketPingFailCount = 0
    private(set) var webSocketAuthenticationFailCount = 0

    private(set) var pollingTimer: Timer?

    let pollInterval: TimeInterval

    public var pollingActive: Bool {
        pollingTimer != nil
    }

    private(set) var networkReachability: NKTypeReachability = .unknown {
        didSet {
            if networkReachability == .notReachable {
                logger.info("Network unreachable, stopping websocket and stopping polling")
                stopPollingTimer()
                resetWebSocket()
            } else if oldValue == .notReachable {
                logger.info("Network reachable, trying to reconnect to websocket")
                reconnectWebSocket()
                startWorkingSetCheck()
            }
        }
    }

    public init(
        account: Account,
        remoteInterface: RemoteInterface,
        changeNotificationInterface: ChangeNotificationInterface,
        domain: NSFileProviderDomain?,
        dbManager: FilesDatabaseManager,
        pollInterval: TimeInterval = 60,
        log: any FileProviderLogging
    ) {
        self.account = account
        self.remoteInterface = remoteInterface
        self.changeNotificationInterface = changeNotificationInterface
        self.domain = domain
        self.dbManager = dbManager
        self.pollInterval = pollInterval
        logger = FileProviderLogger(category: "RemoteChangeObserver", log: log)
        super.init()

        // Authentication fixes require some type of user or external change.
        // We don't want to reset the auth tries within reconnect web socket as this is called
        // internally
        webSocketAuthenticationFailCount = 0

        Task {
            reconnectWebSocket()
        }
    }

    private func startPollingTimer() {
        guard !invalidated else {
            logger.error("Starting polling timer while the current one is not invalidated yet!")
            return
        }

        Task { @MainActor in
            pollingTimer = Timer.scheduledTimer(withTimeInterval: pollInterval, repeats: true) { [weak self] _ in
                self?.logger.info("Polling timer timeout, notifying change.")
                self?.startWorkingSetCheck()
            }

            logger.info("Starting polling timer.")
        }
    }

    private func stopPollingTimer() {
        Task {
            logger.info("Stopping polling timer.")
            pollingTimer?.invalidate()
            pollingTimer = nil
        }
    }

    public func invalidate() {
        logger.debug("Invalidating.")
        invalidated = true
        resetWebSocket()
    }

    private func reconnectWebSocket() {
        logger.debug("Reconnecting web socket...")
        stopPollingTimer()
        resetWebSocket()

        guard networkReachability != .notReachable else {
            logger.error("Network unreachable, will retry when reconnected.")
            return
        }

        guard webSocketAuthenticationFailCount < webSocketAuthenticationFailLimit else {
            logger.error("Exceeded authentication failures for notify push websocket \(account.ncKitAccount), will poll instead.", [.account: account.ncKitAccount])
            startPollingTimer()
            return
        }

        Task { [weak self] in
            try await Task.sleep(nanoseconds: self?.webSocketReconfigureIntervalNanoseconds ?? 0)
            await self?.configureNotifyPush()
        }
    }

    public func resetWebSocket() {
        logger.debug("Resetting web socket...")
        webSocketTask?.cancel()
        webSocketUrlSession = nil
        webSocketTask = nil
        webSocketOperationQueue.cancelAllOperations()
        webSocketOperationQueue.isSuspended = true
        webSocketPingTask?.cancel()
        webSocketPingTask = nil
        webSocketPingFailCount = 0
    }

    private func configureNotifyPush() async {
        logger.debug("Configuring notify push...")

        guard !invalidated else {
            logger.error("Attempt to configure notify push while being invalidated!")
            return
        }

        let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain = self.domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: .rootContainer,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard error == .success else {
            logger.error("Could not get capabilities: \(error.errorCode) \(error.errorDescription)", [.account: account.ncKitAccount])
            reconnectWebSocket()
            return
        }

        guard let capabilities,
              let websocketEndpoint = capabilities.notifyPush?.endpoints?.websocket
        else {
            logger.error("Could not get notifyPush websocket \(account.ncKitAccount), polling.", [.account: account.ncKitAccount])
            startPollingTimer()
            return
        }

        guard let websocketEndpointUrl = URL(string: websocketEndpoint) else {
            logger.error("Received notifyPush endpoint is invalid: \(websocketEndpoint)")

            return
        }
        webSocketOperationQueue.isSuspended = false
        webSocketUrlSession = URLSession(
            configuration: URLSessionConfiguration.default,
            delegate: self,
            delegateQueue: webSocketOperationQueue
        )
        webSocketTask = webSocketUrlSession?.webSocketTask(with: websocketEndpointUrl)
        webSocketTask?.resume()
        logger.info("Successfully configured push notifications for \(account.ncKitAccount)", [.account: account.ncKitAccount])
    }

    func incrementWebSocketPingFailCount() {
        webSocketPingFailCount += 1
    }

    func setNetworkReachability(_ typeReachability: NKTypeReachability) {
        networkReachability = typeReachability
    }

    private func authenticateWebSocket() async {
        guard !invalidated else {
            return
        }

        do {
            try await webSocketTask?.send(.string(account.username))
            try await webSocketTask?.send(.string(account.password))
        } catch {
            logger.error("Error authenticating websocket.", [.account: account.ncKitAccount, .error: error])
        }

        readWebSocket()
    }

    private func startNewWebSocketPingTask() {
        guard !Task.isCancelled, !invalidated else {
            return
        }

        if let webSocketPingTask, !webSocketPingTask.isCancelled {
            webSocketPingTask.cancel()
        }

        let account = accountId

        webSocketPingTask = Task.detached(priority: .background) {
            do {
                try await Task.sleep(nanoseconds: self.webSocketPingIntervalNanoseconds)
            } catch {
                self.logger.error("Could not sleep websocket ping.", [.account: account, .error: error])
            }

            guard !Task.isCancelled else {
                return
            }

            Task {
                self.pingWebSocket()
            }
        }
    }

    private func pingWebSocket() { // Keep the socket connection alive
        guard !invalidated else {
            return
        }

        guard networkReachability != .notReachable else {
            logger.error("Not pinging because network is unreachable.", [.account: account.ncKitAccount])
            return
        }

        webSocketTask?.sendPing { error in
            Task { [weak self] in
                guard let self else {
                    return
                }

                guard await invalidated == false else {
                    return
                }

                guard error == nil else {
                    logger.error("Websocket ping failed.", [.error: error])
                    incrementWebSocketPingFailCount()

                    if webSocketPingFailCount > webSocketPingFailLimit {
                        Task.detached(priority: .medium) {
                            self.reconnectWebSocket()
                        }
                    } else {
                        startNewWebSocketPingTask()
                    }

                    return
                }

                startNewWebSocketPingTask()
            }
        }
    }

    private func readWebSocket() {
        guard !invalidated else {
            return
        }

        webSocketTask?.receive { result in
            Task { [weak self] in
                guard let self else {
                    return
                }

                switch result {
                    case .failure:
                        let accountId = accountId
                        logger.debug("Failed to read websocket.", [.account: accountId])
                    // Do not reconnect here, delegate methods will handle reconnecting
                    case let .success(message):
                        switch message {
                            case let .data(data):
                                processWebsocket(data: data)
                            case let .string(string):
                                processWebsocket(string: string)
                            @unknown default:
                                logger.error("Unknown case encountered while reading websocket!")
                        }

                        readWebSocket()
                }
            }
        }
    }

    private func processWebsocket(data: Data) {
        guard !invalidated else { return }
        guard let string = String(data: data, encoding: .utf8) else {
            logger.error("Could parse websocket data for id: \(account.ncKitAccount)", [.account: account.ncKitAccount])
            return
        }
        processWebsocket(string: string)
    }

    private func processWebsocket(string: String) {
        logger.debug("Received websocket string: \(string)")
        if string == "notify_file" {
            logger.debug("Received file notification for \(account.ncKitAccount)", [.account: account.ncKitAccount])
            startWorkingSetCheck()
        } else if string == "notify_activity" {
            logger.debug("Ignoring activity notification: \(account.ncKitAccount)", [.account: account.ncKitAccount])
        } else if string == "notify_notification" {
            logger.debug("Ignoring notification: \(account.ncKitAccount)", [.account: account.ncKitAccount])
        } else if string == "authenticated" {
            logger.debug("Correctly authed websocket \(account.ncKitAccount), pinging", [.account: account.ncKitAccount])
            NotificationCenter.default.post(
                name: NotifyPushAuthenticatedNotificationName, object: self
            )
            startNewWebSocketPingTask()
        } else if string == "err: Invalid credentials" {
            logger.debug(
                """
                Invalid creds for websocket for \(account.ncKitAccount),
                reattempting auth.
                """
            )
            webSocketAuthenticationFailCount += 1
            reconnectWebSocket()
        } else {
            logger.error("Received unknown string from websocket \(account.ncKitAccount): \(string)", [.account: account.ncKitAccount])
        }
    }

    func replaceAccount(with account: Account) {
        self.account = account
    }

    func setWebSocketPingInterval(to nanoseconds: UInt64) {
        webSocketPingIntervalNanoseconds = nanoseconds
    }
}

// MARK: - URLSessionWebSocketDelegate

extension RemoteChangeObserver: URLSessionWebSocketDelegate {
    public nonisolated func urlSession(_: URLSession, webSocketTask _: URLSessionWebSocketTask, didOpenWithProtocol _: String?) {
        Task {
            guard invalidated == false else {
                return
            }

            logger.debug("Websocket connected sending auth details", [.account: accountId])
            await authenticateWebSocket()
        }
    }

    public nonisolated func urlSession(_: URLSession, webSocketTask: URLSessionWebSocketTask, didCloseWith _: URLSessionWebSocketTask.CloseCode, reason: Data?) {
        Task {
            guard invalidated == false else {
                return
            }

            // If the task that closed is not the current active task, it means we have
            // already initiated a reset and this is a stale callback. Ignore it.
            guard webSocketTask === self.webSocketTask else {
                logger.debug("An old websocket task closed, ignoring.")
                return
            }

            logger.debug("Socket connection closed: \(String(data: reason ?? Data(), encoding: .utf8) ?? "unknown reason"). Retrying websocket connection.", [.account: accountId])
            reconnectWebSocket()
        }
    }

    public nonisolated func urlSessionDidFinishEvents(forBackgroundURLSession _: URLSession) {}
}

// MARK: - NextcloudKitDelegate methods

extension RemoteChangeObserver: NextcloudKitDelegate {
    public nonisolated func authenticationChallenge(_: URLSession, didReceive challenge: URLAuthenticationChallenge, completionHandler: @Sendable @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void) {
        Task { [weak self] in
            guard let self else {
                return
            }

            guard !invalidated else {
                return
            }

            let authMethod = challenge.protectionSpace.authenticationMethod
            logger.debug("Received auth challenge with method: \(authMethod)")

            if authMethod == NSURLAuthenticationMethodHTTPBasic {
                let credential = URLCredential(user: account.username, password: account.password, persistence: .forSession)
                completionHandler(.useCredential, credential)
            } else if authMethod == NSURLAuthenticationMethodServerTrust {
                // TODO: Validate the server trust
                guard let serverTrust = challenge.protectionSpace.serverTrust else {
                    logger.error("Received server trust auth challenge but no trust avail")
                    completionHandler(.cancelAuthenticationChallenge, nil)
                    return
                }

                let credential = URLCredential(trust: serverTrust)
                completionHandler(.useCredential, credential)
            } else {
                logger.error("Unhandled auth method: \(authMethod)")
                // Handle other authentication methods or cancel the challenge
                completionHandler(.performDefaultHandling, nil)
            }
        }
    }

    public nonisolated func networkReachabilityObserver(_ typeReachability: NKTypeReachability) {
        Task { [weak self] in
            guard let self else {
                return
            }

            setNetworkReachability(typeReachability)
        }
    }

    public nonisolated func downloadProgress(
        _: Float,
        totalBytes _: Int64,
        totalBytesExpected _: Int64,
        fileName _: String,
        serverUrl _: String,
        session _: URLSession,
        task _: URLSessionTask
    ) {}

    public nonisolated func uploadProgress(
        _: Float,
        totalBytes _: Int64,
        totalBytesExpected _: Int64,
        fileName _: String,
        serverUrl _: String,
        session _: URLSession,
        task _: URLSessionTask
    ) {}

    public nonisolated func downloadingFinish(
        _: URLSession,
        downloadTask _: URLSessionDownloadTask,
        didFinishDownloadingTo _: URL
    ) {}

    public nonisolated func downloadComplete(
        fileName _: String,
        serverUrl _: String,
        etag _: String?,
        date _: Date?,
        dateLastModified _: Date?,
        length _: Int64,
        task _: URLSessionTask,
        error _: NKError
    ) {}

    public nonisolated func uploadComplete(
        fileName _: String,
        serverUrl _: String,
        ocId _: String?,
        etag _: String?,
        date _: Date?,
        size _: Int64,
        task _: URLSessionTask,
        error _: NKError
    ) {}

    public nonisolated func request(_: Alamofire.DataRequest, didParseResponse _: Alamofire.AFDataResponse<some Any>) {}

    ///
    /// Dispatches the asynchronous working set check.
    ///
    /// - Parameters:
    ///     - completionHandler: An optional closure to call after the working set check completed.
    ///
    func startWorkingSetCheck(completionHandler: (@Sendable () -> Void)? = nil) {
        guard !workingSetCheckOngoing, !invalidated else {
            logger.error("Cancelling dispatch of working set check because it either is already ongoing or this is invalidated!")
            return
        }

        Task {
            await checkWorkingSet()
            completionHandler?()
        }
    }

    private func checkWorkingSet() async {
        logger.debug("Checking working set...")
        workingSetCheckOngoing = true

        defer {
            logger.debug("Working set check no longer ongoing.")
            workingSetCheckOngoing = false
        }

        // Unlike when enumerating items we can't progressively enumerate items as we need to
        // wait to see which items are truly deleted and which have just been moved elsewhere.
        // Visited folders and downloaded files. Sort in terms of their remote URLs.
        // This way we ensure we visit parent folders before their children.
        let materialisedItems = dbManager
            .materialisedItemMetadatas(account: account.ncKitAccount)
            .sorted { $0.remotePath().count < $1.remotePath().count }

        var allNewMetadatas = [SendableItemMetadata]()
        var allUpdatedMetadatas = [SendableItemMetadata]()
        var allDeletedMetadatas = [SendableItemMetadata]()
        var examinedItemIds = Set<String>()

        for item in materialisedItems where !examinedItemIds.contains(item.ocId) {
            guard !invalidated else {
                return
            }

            guard isLockFileName(item.fileName) == false else {
                // Skip server requests for locally created lock files.
                // They are not synchronized to the server for real.
                // Thus they can be expected not to be found there.
                // That would also cause their local deletion due to synchronization logic.
                logger.debug("Skipping materialized item in working set check because the name hints a lock file.", [.item: item, .name: item.name])
                continue
            }

            let itemRemoteUrl = item.remotePath()

            let (metadatas, newMetadatas, updatedMetadatas, deletedMetadatas, _, readError) = await Enumerator.readServerUrl(
                itemRemoteUrl,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                depth: item.directory ? .targetAndDirectChildren : .target,
                log: logger.log
            )

            guard !invalidated else {
                return
            }

            if readError?.errorCode == 404 {
                allDeletedMetadatas.append(item)
                examinedItemIds.insert(item.ocId)
                materialisedItems
                    .filter { $0.serverUrl == itemRemoteUrl }
                    .forEach {
                        allDeletedMetadatas.append($0)
                        examinedItemIds.insert(item.ocId)
                    }
            } else if let readError, readError != .success {
                logger.info("Finished change enumeration of working set for user \(account.ncKitAccount) with error.", [.account: account.ncKitAccount, .error: readError])
                return
            } else {
                allDeletedMetadatas += deletedMetadatas ?? []
                allUpdatedMetadatas += updatedMetadatas ?? []
                allNewMetadatas += newMetadatas ?? []

                // Just because we have read child directories metadata doesn't mean we need
                // to in turn scan their children. This is not the case for files
                var examinedChildFilesAndDeletedItems = Set<String>()
                if let metadatas, let target = metadatas.first {
                    examinedItemIds.insert(target.ocId)

                    if metadatas.count > 1 {
                        examinedChildFilesAndDeletedItems.formUnion(
                            metadatas[1...].filter { !$0.directory }.map(\.ocId)
                        )
                    }

                    // If the target is not in the updated metadatas then neither it, nor
                    // any of its kids have changed. So skip examining all of them
                    if !allUpdatedMetadatas.contains(where: { $0.ocId == target.ocId }) {
                        logger.debug("Target \(itemRemoteUrl) has not changed. Skipping children")
                        let materialisedChildren = materialisedItems.filter {
                            $0.serverUrl.hasPrefix(itemRemoteUrl)
                        }.map(\.ocId)
                        examinedChildFilesAndDeletedItems.formUnion(materialisedChildren)
                    }

                    // OPTIMIZATION: For any child directories returned in this enumeration,
                    // if they haven't changed (etag matches database), mark them as examined
                    // so we don't enumerate them separately later
                    if metadatas.count > 1 {
                        let childDirectories = metadatas[1...].filter(\.directory)
                        for childDir in childDirectories {
                            // Check if this directory is in our materialized items list
                            if let localItem = materialisedItems.first(where: { $0.ocId == childDir.ocId }),
                               localItem.etag == childDir.etag
                            {
                                // Directory hasn't changed, mark as examined to skip separate enumeration
                                logger.debug("Child directory \(childDir.fileName) etag unchanged (\(childDir.etag)), marking as examined")
                                examinedChildFilesAndDeletedItems.insert(childDir.ocId)

                                // Also mark any materialized children of this directory as examined
                                let grandChildren = materialisedItems.filter {
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
        guard !invalidated else { return }

        // Run a check to ensure files deleted in one location are not updated in another
        // (e.g. when moved)
        // The recursive scan provides us with updated/deleted metadatas only on a folder by
        // folder basis; so we need to check we are not simultaneously marking a moved file as
        // deleted and updated
        var checkedDeletedMetadatas = allDeletedMetadatas

        for updatedMetadata in allUpdatedMetadatas {
            guard let matchingDeletedMetadataIdx = checkedDeletedMetadatas.firstIndex(
                where: { $0.ocId == updatedMetadata.ocId }
            ) else { continue }
            checkedDeletedMetadatas.remove(at: matchingDeletedMetadataIdx)
        }

        allDeletedMetadatas = checkedDeletedMetadatas

        for deletedMetadata in allDeletedMetadatas {
            var deleteMarked = deletedMetadata
            deleteMarked.deleted = true
            deleteMarked.syncTime = Date()
            dbManager.addItemMetadata(deleteMarked)
        }

        logger.info("Finished change enumeration of working set. Examined item IDs: \(examinedItemIds), materialized item IDs: \(materialisedItems.map(\.ocId))")

        if allUpdatedMetadatas.isEmpty, allDeletedMetadatas.isEmpty {
            logger.info("No changes found.")
        } else {
            changeNotificationInterface.notifyChange()
        }
    }
}

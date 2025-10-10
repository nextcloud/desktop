//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Alamofire
@preconcurrency import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit

public let NotifyPushAuthenticatedNotificationName = Notification.Name("NotifyPushAuthenticated")

final public class RemoteChangeObserver: NSObject, NextcloudKitDelegate, URLSessionWebSocketDelegate {
    public let remoteInterface: RemoteInterface
    public let changeNotificationInterface: ChangeNotificationInterface
    public let domain: NSFileProviderDomain?
    public let dbManager: FilesDatabaseManager
    public var account: Account
    public var accountId: String { account.ncKitAccount }

    public var webSocketPingIntervalNanoseconds: UInt64 = 3 * 1_000_000_000
    public var webSocketReconfigureIntervalNanoseconds: UInt64 = 1 * 1_000_000_000
    public var webSocketPingFailLimit = 8
    public var webSocketAuthenticationFailLimit = 3
    public var webSocketTaskActive: Bool { webSocketTask != nil }

    private let logger: FileProviderLogger

    private var workingSetCheckOngoing = false
    private var invalidated = false

    private var webSocketUrlSession: URLSession?
    private var webSocketTask: URLSessionWebSocketTask?
    private var webSocketOperationQueue = OperationQueue()
    private var webSocketPingTask: Task<(), Never>?
    private(set) var webSocketPingFailCount = 0
    private(set) var webSocketAuthenticationFailCount = 0

    private(set) var pollingTimer: Timer?
    public var pollInterval: TimeInterval = 60 {
        didSet {
            if pollingActive {
                stopPollingTimer()
                startPollingTimer()
            }
        }
    }
    public var pollingActive: Bool { pollingTimer != nil }

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
        log: any FileProviderLogging
    ) {
        self.account = account
        self.remoteInterface = remoteInterface
        self.changeNotificationInterface = changeNotificationInterface
        self.domain = domain
        self.dbManager = dbManager
        self.logger = FileProviderLogger(category: "RemoteChangeObserver", log: log)
        super.init()
        connect()
    }

    private func startPollingTimer() {
        guard !invalidated else { return }
        Task { @MainActor in
            pollingTimer = Timer.scheduledTimer(
                withTimeInterval: pollInterval, repeats: true
            ) { [weak self] timer in
                self?.logger.info("Polling timer timeout, notifying change.")
                self?.startWorkingSetCheck()
            }
            logger.info("Starting polling timer.")
        }
    }

    private func stopPollingTimer() {
        Task { @MainActor in
            logger.info("Stopping polling timer.")
            pollingTimer?.invalidate()
            pollingTimer = nil
        }
    }

    public func invalidate() {
        invalidated = true
        resetWebSocket()
    }

    public func connect() {
        // Authentication fixes require some type of user or external change.
        // We don't want to reset the auth tries within reconnect web socket as this is called
        // internally
        webSocketAuthenticationFailCount = 0
        reconnectWebSocket()
    }

    private func reconnectWebSocket() {
        stopPollingTimer()
        resetWebSocket()
        guard networkReachability != .notReachable else {
            logger.error("Network unreachable, will retry when reconnected.")
            return
        }
        guard webSocketAuthenticationFailCount < webSocketAuthenticationFailLimit else {
            logger.error("Exceeded authentication failures for notify push websocket \(accountId), will poll instead.", [.account: accountId])
            startPollingTimer()
            return
        }
        Task { [weak self] in
            try await Task.sleep(nanoseconds: self?.webSocketReconfigureIntervalNanoseconds ?? 0)
            await self?.configureNotifyPush()
        }
    }

    public func resetWebSocket() {
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
        guard !invalidated else { return }
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
            self.logger.error("Could not get capabilities: \(error.errorCode) \(error.errorDescription)", [.account: accountId])
            reconnectWebSocket()
            return
        }

        guard let capabilities,
              let websocketEndpoint = capabilities.notifyPush?.endpoints?.websocket
        else {
            logger.error("Could not get notifyPush websocket \(self.accountId), polling.", [.account: accountId])
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
        logger.info("Successfully configured push notifications for \(accountId)", [.account: accountId])
    }

    public func authenticationChallenge(
        _ session: URLSession,
        didReceive challenge: URLAuthenticationChallenge,
        completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void
    ) {
        guard !invalidated else { return }
        let authMethod = challenge.protectionSpace.authenticationMethod
        logger.debug("Received auth challenge with method: \(authMethod)")
        if authMethod == NSURLAuthenticationMethodHTTPBasic {
            let credential = URLCredential(
                user: account.username,
                password: account.password,
                persistence: .forSession
            )
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

    public func urlSession(
        _ session: URLSession,
        webSocketTask: URLSessionWebSocketTask,
        didOpenWithProtocol protocol: String?
    ) {
        guard !invalidated else { return }
        logger.debug("Websocket connected \(accountId) sending auth details", [.account: accountId])
        Task { await authenticateWebSocket() }
    }

    public func urlSession(
        _ session: URLSession,
        webSocketTask: URLSessionWebSocketTask,
        didCloseWith closeCode: URLSessionWebSocketTask.CloseCode,
        reason: Data?
    ) {
        guard !invalidated else { return }
        // If the task that closed is not the current active task, it means we have
        // already initiated a reset and this is a stale callback. Ignore it.
        guard webSocketTask === self.webSocketTask else {
            logger.debug("An old websocket task closed, ignoring.")
            return
        }

        logger.debug("Socket connection closed for \(accountId).", [.account: accountId])

        if let reason = reason {
            logger.debug("Reason: \(String(data: reason, encoding: .utf8) ?? "")")
        }

        logger.debug("Retrying websocket connection for \(accountId).", [.account: accountId])
        reconnectWebSocket()
    }

    private func authenticateWebSocket() async {
        guard !invalidated else {
            return
        }

        do {
            try await webSocketTask?.send(.string(account.username))
            try await webSocketTask?.send(.string(account.password))
        } catch let error {
            logger.error("Error authenticating websocket.", [.account: accountId, .error: error])
        }

        readWebSocket()
    }

    private func startNewWebSocketPingTask() {
        guard !Task.isCancelled, !invalidated else { return }

        if let webSocketPingTask, !webSocketPingTask.isCancelled {
            webSocketPingTask.cancel()
        }

        webSocketPingTask = Task.detached(priority: .background) {
            do {
                try await Task.sleep(nanoseconds: self.webSocketPingIntervalNanoseconds)
            } catch let error {
                self.logger.error("Could not sleep websocket ping.", [.account: self.accountId, .error: error])
            }
            guard !Task.isCancelled else { return }
            self.pingWebSocket()
        }
    }

    private func pingWebSocket() {  // Keep the socket connection alive
        guard !invalidated else { return }
        guard networkReachability != .notReachable else {
            logger.error("Not pinging because network is unreachable.", [.account: self.accountId])
            return
        }

        webSocketTask?.sendPing { [weak self] error in
            guard let self, !self.invalidated else { return }
            guard error == nil else {
                self.logger.error("Websocket ping failed.", [.error: error])
                self.webSocketPingFailCount += 1
                if self.webSocketPingFailCount > self.webSocketPingFailLimit {
                    Task.detached(priority: .medium) { self.reconnectWebSocket() }
                } else {
                    self.startNewWebSocketPingTask()
                }
                return
            }

            self.startNewWebSocketPingTask()
        }
    }

    private func readWebSocket() {
        guard !invalidated else { return }
        webSocketTask?.receive { result in
            switch result {
            case .failure:
                    self.logger.debug("Failed to read websocket \(self.accountId)", [.account: self.accountId])
                // Do not reconnect here, delegate methods will handle reconnecting
            case .success(let message):
                switch message {
                case .data(let data):
                    self.processWebsocket(data: data)
                case .string(let string):
                    self.processWebsocket(string: string)
                @unknown default:
                    self.logger.error("Unknown case encountered while reading websocket!")
                }
                self.readWebSocket()
            }
        }
    }

    private func processWebsocket(data: Data) {
        guard !invalidated else { return }
        guard let string = String(data: data, encoding: .utf8) else {
            logger.error("Could parse websocket data for id: \(accountId)", [.account: accountId])
            return
        }
        processWebsocket(string: string)
    }

    private func processWebsocket(string: String) {
        logger.debug("Received websocket string: \(string)")
        if string == "notify_file" {
            logger.debug("Received file notification for \(accountId)", [.account: accountId])
            startWorkingSetCheck()
        } else if string == "notify_activity" {
            logger.debug("Ignoring activity notification: \(accountId)", [.account: accountId])
        } else if string == "notify_notification" {
            logger.debug("Ignoring notification: \(accountId)", [.account: accountId])
        } else if string == "authenticated" {
            logger.debug("Correctly authed websocket \(accountId), pinging", [.account: accountId])
            NotificationCenter.default.post(
                name: NotifyPushAuthenticatedNotificationName, object: self
            )
            startNewWebSocketPingTask()
        } else if string == "err: Invalid credentials" {
            logger.debug(
                """
                Invalid creds for websocket for \(self.accountId),
                reattempting auth.
                """
            )
            webSocketAuthenticationFailCount += 1
            reconnectWebSocket()
        } else {
            logger.error("Received unknown string from websocket \(accountId): \(string)", [.account: accountId])
        }
    }

    // MARK: - NextcloudKitDelegate methods

    public func networkReachabilityObserver(_ typeReachability: NKTypeReachability) {
        networkReachability = typeReachability
    }

    public func urlSessionDidFinishEvents(forBackgroundURLSession session: URLSession) { }

    public func downloadProgress(
        _ progress: Float,
        totalBytes: Int64,
        totalBytesExpected: Int64,
        fileName: String,
        serverUrl: String,
        session: URLSession,
        task: URLSessionTask
    ) { }

    public func uploadProgress(
        _ progress: Float,
        totalBytes: Int64,
        totalBytesExpected: Int64,
        fileName: String,
        serverUrl: String,
        session: URLSession,
        task: URLSessionTask
    ) { }

    public func downloadingFinish(
        _ session: URLSession,
        downloadTask: URLSessionDownloadTask,
        didFinishDownloadingTo location: URL
    ) { }

    public func downloadComplete(
        fileName: String,
        serverUrl: String,
        etag: String?,
        date: Date?,
        dateLastModified: Date?,
        length: Int64,
        task: URLSessionTask,
        error: NKError
    ) { }

    public func uploadComplete(
        fileName: String,
        serverUrl: String,
        ocId: String?,
        etag: String?,
        date: Date?,
        size: Int64,
        task: URLSessionTask,
        error: NKError
    ) { }

    public func request<Value>(
        _ request: Alamofire.DataRequest, didParseResponse response: Alamofire.AFDataResponse<Value>
    ) { }

    ///
    /// Dispatches the asynchronous working set check.
    ///
    /// - Parameters:
    ///     - completionHandler: An optional closure to call after the working set check completed.
    ///
    func startWorkingSetCheck(completionHandler: (() -> Void)? = nil) {
        guard !workingSetCheckOngoing, !invalidated else { return }
        Task {
            await checkWorkingSet()
            completionHandler?()
        }
    }

    private func checkWorkingSet() async {
        workingSetCheckOngoing = true
        
        defer {
            workingSetCheckOngoing = false
        }

        // Unlike when enumerating items we can't progressively enumerate items as we need to
        // wait to see which items are truly deleted and which have just been moved elsewhere.
        // Visited folders and downloaded files. Sort in terms of their remote URLs.
        // This way we ensure we visit parent folders before their children.
        let materialisedItems = dbManager
            .materialisedItemMetadatas(account: accountId)
            .sorted {
                ($0.serverUrl + "/" + $0.fileName).count <
                    ($1.serverUrl + "/" + $1.fileName).count
            }


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
            
            let itemRemoteUrl = item.serverUrl + "/" + item.fileName
            
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
                logger.info("Finished change enumeration of working set for user \(accountId) with error.", [.account: accountId, .error: readError])
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
                        let childDirectories = metadatas[1...].filter { $0.directory }
                        for childDir in childDirectories {
                            // Check if this directory is in our materialized items list
                            if let localItem = materialisedItems.first(where: { $0.ocId == childDir.ocId }),
                               localItem.etag == childDir.etag {
                                // Directory hasn't changed, mark as examined to skip separate enumeration
                                logger.debug("Child directory \(childDir.fileName) etag unchanged (\(childDir.etag)), marking as examined")
                                examinedChildFilesAndDeletedItems.insert(childDir.ocId)
                                
                                // Also mark any materialized children of this directory as examined
                                let grandChildren = materialisedItems.filter {
                                    $0.serverUrl.hasPrefix(localItem.serverUrl + "/" + localItem.fileName)
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
        let task = Task { @MainActor in
            allDeletedMetadatas.forEach {
                var deleteMarked = $0
                deleteMarked.deleted = true
                deleteMarked.syncTime = Date()
                dbManager.addItemMetadata(deleteMarked)
            }
        }
        _ = await task.result

        logger.info("Finished change enumeration of working set. Examined item IDs: \(examinedItemIds), materialized item IDs: \(materialisedItems.map(\.ocId))")

        if allUpdatedMetadatas.isEmpty, allDeletedMetadatas.isEmpty {
            logger.info("No changes found.")
        } else {
            changeNotificationInterface.notifyChange()
        }
    }
}

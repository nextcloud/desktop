//
//  RemoteChangeObserver.swift
//
//
//  Created by Claudio Cambra on 17/4/24.
//

import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit
import OSLog

fileprivate let NotifyPushWebSocketPingIntervalNanoseconds: UInt64 = 30 * 1_000_000
fileprivate let NotifyPushWebSocketPingFailLimit = 8
fileprivate let NotifyPushWebSocketAuthenticationFailLimit = 3

public class RemoteChangeObserver:
    NSObject, NKCommonDelegate, URLSessionDelegate, URLSessionWebSocketDelegate
{
    public let ncKit: NextcloudKit
    public let domain: NSFileProviderDomain
    public var ncAccount: String { ncKit.nkCommonInstance.account }

    private let logger = Logger(subsystem: Logger.subsystem, category: "changeobserver")

    private var webSocketUrlSession: URLSession?
    private var webSocketTask: URLSessionWebSocketTask?
    private var webSocketOperationQueue = OperationQueue()
    private var webSocketPingFailCount = 0
    private var webSocketAuthenticationFailCount = 0

    private var pollingTimer: Timer?

    private var networkReachability: NKCommon.TypeReachability = .unknown {
        didSet {
            if oldValue == .notReachable, networkReachability != .notReachable {
                reconnectWebSocket()
                signalEnumerator()
            }
        }
    }

    public init(ncKit: NextcloudKit, domain: NSFileProviderDomain) {
        self.ncKit = ncKit
        self.domain = domain
        super.init()
        reconnectWebSocket()
    }

    private func startPollingTimer() {
        pollingTimer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { _ in
            self.signalEnumerator()
        }
    }

    private func stopPollingTimer() {
        pollingTimer?.invalidate()
        pollingTimer = nil
    }

    private func signalEnumerator() {
        NSFileProviderManager(for: domain)?.signalEnumerator(for: .rootContainer) { error in
            if let error = error {
                self.logger.error(
                    """
                    Could not signal enumerator for \(self.ncAccount, privacy: .public):
                    \(error.localizedDescription, privacy: .public)
                    """
                )
            }
        }
    }

    private func reconnectWebSocket() {
        stopPollingTimer()
        resetWebSocket()
        guard networkReachability != .notReachable else {
            logger.error("Network unreachable, will retry when reconnected")
            return
        }
        guard webSocketAuthenticationFailCount < NotifyPushWebSocketAuthenticationFailLimit else {
            logger.error(
                """
                Exceeded authentication failures for notify push websocket
                \(self.ncAccount, privacy: .public)
                """
            )
            return
        }
        Task { await self.configureNotifyPush() }
    }

    private func resetWebSocket() {
        webSocketUrlSession = nil
        webSocketTask = nil
        webSocketOperationQueue.cancelAllOperations()
        webSocketOperationQueue.isSuspended = true
        webSocketPingFailCount = 0
    }

    private func configureNotifyPush() async {
        let capabilitiesData: Data? = await withCheckedContinuation { continuation in
            ncKit.getCapabilities { account, data, error in
                guard error == .success else {
                    self.logger.error(
                        """
                        Could not get \(self.ncAccount, privacy: .public) capabilities:
                        \(error.errorCode, privacy: .public)
                        \(error.errorDescription, privacy: .public)
                        """
                    )
                    continuation.resume(returning: nil)
                    return
                }
                continuation.resume(returning: data)
            }
        }
        guard let capabilitiesData = capabilitiesData,
              let capabilities = Capabilities(data: capabilitiesData),
              let websocketEndpoint = capabilities.notifyPush?.endpoints?.websocket
        else {
            logger.error(
                """
                Could not get notifyPush websocket \(self.ncAccount, privacy: .public), polling.
                """
            )
            startPollingTimer()
            return
        }

        guard let websocketEndpointUrl = URL(string: websocketEndpoint) else {
            logger.error(
                """
                Received notifyPush endpoint is invalid: \(websocketEndpoint, privacy: .public)
                """
            )
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
        logger.info(
            """
            Successfully configured push notifications for \(self.ncAccount, privacy: .public)
            """
        )
    }

    public func urlSession(
        _ session: URLSession,
        didReceive challenge: URLAuthenticationChallenge,
        completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void
    ) {
        let authMethod = challenge.protectionSpace.authenticationMethod
        logger.debug("Received auth challenge with method: \(authMethod, privacy: .public)")
        if authMethod == NSURLAuthenticationMethodHTTPBasic {
            let credential = URLCredential(
                user: ncKit.nkCommonInstance.userId,
                password: ncKit.nkCommonInstance.password,
                persistence: .forSession
            )
            completionHandler(.useCredential, credential)
        } else if authMethod == NSURLAuthenticationMethodServerTrust {
            // TODO: Validate the server trust
            guard let serverTrust = challenge.protectionSpace.serverTrust else {
                logger.warning("Received server trust auth challenge but no trust avail")
                completionHandler(.cancelAuthenticationChallenge, nil)
                return
            }
            let credential = URLCredential(trust: serverTrust)
            completionHandler(.useCredential, credential)
        } else {
            logger.warning("Unhandled auth method: \(authMethod, privacy: .public)")
            // Handle other authentication methods or cancel the challenge
            completionHandler(.cancelAuthenticationChallenge, nil)
        }
    }

    public func urlSession(
        _ session: URLSession,
        webSocketTask: URLSessionWebSocketTask,
        didOpenWithProtocol protocol: String?
    ) {
        logger.debug("Websocket connected \(self.ncAccount, privacy: .public) sending auth details")
        Task { await authenticateWebSocket() }
    }

    public func urlSession(
        _ session: URLSession,
        webSocketTask: URLSessionWebSocketTask,
        didCloseWith closeCode: URLSessionWebSocketTask.CloseCode,
        reason: Data?
    ) {
        logger.debug("Socket connection closed for \(self.ncAccount, privacy: .public).")
        if let reason = reason {
            logger.debug("Reason: \(String(data: reason, encoding: .utf8) ?? "", privacy: .public)")
        }
        logger.debug("Retrying websocket connection for \(self.ncAccount, privacy: .public).")
        reconnectWebSocket()
    }

    private func authenticateWebSocket() async {
        do {
            try await webSocketTask?.send(.string(ncKit.nkCommonInstance.userId))
            try await webSocketTask?.send(.string(ncKit.nkCommonInstance.password))
        } catch let error {
            logger.error(
                """
                Error authenticating websocket for \(self.ncAccount, privacy: .public):
                \(error.localizedDescription, privacy: .public)
                """
            )
        }
        readWebSocket()
    }

    private func pingWebSocket() {  // Keep the socket connection alive
        guard networkReachability != .notReachable else {
            logger.error("Not pinging \(self.ncAccount, privacy: .public), network is unreachable")
            return
        }

        webSocketTask?.sendPing { error in
            guard error == nil else {
                self.logger.warning(
                    """
                    Websocket ping failed: \(error?.localizedDescription ?? "", privacy: .public)
                    """
                )
                self.webSocketPingFailCount += 1
                if self.webSocketPingFailCount > NotifyPushWebSocketPingFailLimit {
                    self.reconnectWebSocket()
                } else {
                    self.pingWebSocket()
                }
                return
            }

            // TODO: Stop on auth change
            Task {
                do {
                    try await Task.sleep(nanoseconds: NotifyPushWebSocketPingIntervalNanoseconds)
                } catch let error {
                    self.logger.error(
                        """
                        Could not sleep websocket ping for \(self.ncAccount, privacy: .public):
                        \(error.localizedDescription, privacy: .public)
                        """
                    )
                }
                self.pingWebSocket()
            }
        }
    }

    private func readWebSocket() {
        webSocketTask?.receive { result in
            switch result {
            case .failure:
                self.logger.debug("Failed to read websocket \(self.ncAccount, privacy: .public)")
                self.reconnectWebSocket()
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
        guard let string = String(data: data, encoding: .utf8) else {
            logger.error("Could parse websocket data for id: \(self.ncAccount, privacy: .public)")
            return
        }
        processWebsocket(string: string)
    }

    private func processWebsocket(string: String) {
        logger.debug("Received websocket string: \(string, privacy: .public)")
        if string == "notify_file" {
            logger.debug("Received file notification for \(self.ncAccount, privacy: .public)")
            signalEnumerator()
        } else if string == "notify_activity" {
            logger.debug("Ignoring activity notification: \(self.ncAccount, privacy: .public)")
        } else if string == "notify_notification" {
            logger.debug("Ignoring notification: \(self.ncAccount, privacy: .public)")
        } else if string == "authenticated" {
            logger.debug("Correctly authed websocket \(self.ncAccount, privacy: .public), pinging")
            pingWebSocket()
        } else if string == "err: Invalid credentials" {
            logger.debug(
                """
                Invalid creds for websocket for \(self.ncAccount, privacy: .public),
                reattempting auth.
                """
            )
            webSocketAuthenticationFailCount += 1
            reconnectWebSocket()
        } else {
            logger.warning(
                """
                Received unknown string from websocket \(self.ncAccount, privacy: .public):
                \(string, privacy: .public)
                """
            )
        }
    }
}

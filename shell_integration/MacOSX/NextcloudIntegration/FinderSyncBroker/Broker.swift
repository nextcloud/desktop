/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import Foundation
import os

///
/// Rendezvous point between the desktop client and its FinderSync extension.
///
/// See `FinderSyncBrokerProtocol.h` for why this process has to exist: neither the app nor
/// the extension may publish a named XPC endpoint, because only a launchd job can, and this
/// login item is one. The broker vends the name, stores the endpoint of the app's anonymous
/// listener, and hands it to the extension. Nothing else flows through here — once the
/// extension has the endpoint it talks to the app directly.
///
/// Roles are not declared by the peers, they are inferred from behaviour: whichever
/// connection calls ``publishAppEndpoint(_:reply:)`` is the app, and any connection that
/// calls ``fetchAppEndpoint(reply:)`` is an extension and is added to the push set. That
/// avoids a separate identify-yourself handshake and cannot be spoofed into anything
/// dangerous, because every connection has already had to satisfy `PeerRequirement`.
///
/// `@unchecked Sendable`: all mutable state below is guarded by `lock`, and every method is
/// safe to call from the arbitrary queues that `NSXPCListener` and `NSXPCConnection` use.
/// An `actor` is not usable here because `listener(_:shouldAcceptNewConnection:)` is a
/// synchronous, nonisolated callback that must return `Bool` without suspending.
///
final class Broker: NSObject, NSXPCListenerDelegate, FinderSyncBrokerProtocol, @unchecked Sendable {
    private let log: Logger
    private let brokerVersion: String

    private let lock = NSLock()

    /// Endpoint most recently published by the app, if it is still reachable.
    private var appEndpoint: NSXPCListenerEndpoint?

    /// Monotonic counter identifying the current endpoint. Never reset, so the extension can
    /// always tell a fresh endpoint from one it has already acted on. Zero means "none".
    private var endpointGeneration: UInt64 = 0

    /// The connection that published the current endpoint.
    private var appConnectionID: ObjectIdentifier?

    /// Every accepted connection, retained until it invalidates. Without this the connection
    /// objects would be deallocated as soon as the delegate callback returned.
    private var connections: [ObjectIdentifier: NSXPCConnection] = [:]

    /// Connections that have asked for an endpoint and therefore expect to be pushed changes.
    private var pushTargets: Set<ObjectIdentifier> = []

    init(log: Logger, brokerVersion: String) {
        self.log = log
        self.brokerVersion = brokerVersion
        super.init()
    }

    // MARK: - NSXPCListenerDelegate

    /// - Note: Peer authentication is not done here. The requirement is installed once on the
    ///   listener with `setConnectionCodeSigningRequirement(_:)`, which makes XPC reject
    ///   connections that fail it *before* this method is called. Repeating it per connection
    ///   would risk setting a requirement twice, which is an XPC error.
    func listener(_ listener: NSXPCListener,
                  shouldAcceptNewConnection newConnection: NSXPCConnection) -> Bool {
        newConnection.exportedInterface = NSXPCInterface(with: FinderSyncBrokerProtocol.self)
        newConnection.exportedObject = self
        newConnection.remoteObjectInterface = NSXPCInterface(with: FinderSyncBrokerClientProtocol.self)

        let id = ObjectIdentifier(newConnection)

        // Capture the identifier by value, never the connection: `connections` retains it, so
        // capturing it in its own handlers would make the pair immortal.
        newConnection.invalidationHandler = { [weak self] in
            self?.forget(id, reason: "invalidated")
        }
        newConnection.interruptionHandler = { [weak self] in
            // Interruption means the peer process died. XPC will not redeliver anything on
            // this connection, so treat it exactly like invalidation.
            self?.forget(id, reason: "interrupted")
        }

        lock.lock()
        connections[id] = newConnection
        let total = connections.count
        lock.unlock()

        newConnection.resume()

        log.info("Accepted XPC connection from pid \(newConnection.processIdentifier, privacy: .public); \(total, privacy: .public) open")
        return true
    }

    // MARK: - FinderSyncBrokerProtocol

    func publishAppEndpoint(_ endpoint: NSXPCListenerEndpoint, reply: @escaping (UInt64) -> Void) {
        guard let connection = NSXPCConnection.current() else {
            // Only reachable if called outside of message handling, which cannot happen for
            // an exported object. Answer with the "nothing published" sentinel rather than
            // storing an endpoint we cannot tie to a connection lifetime.
            log.error("publishAppEndpoint called with no current connection; ignoring")
            reply(0)
            return
        }

        lock.lock()
        endpointGeneration += 1
        appEndpoint = endpoint
        appConnectionID = ObjectIdentifier(connection)
        let generation = endpointGeneration
        let targets = pushTargets.compactMap { connections[$0] }
        lock.unlock()

        log.info("App published endpoint, generation \(generation, privacy: .public); pushing to \(targets.count, privacy: .public) extension(s)")
        reply(generation)

        push(endpoint: endpoint, generation: generation, to: targets)
    }

    func fetchAppEndpoint(reply: @escaping (NSXPCListenerEndpoint?, UInt64) -> Void) {
        var endpoint: NSXPCListenerEndpoint?
        var generation: UInt64 = 0

        if let connection = NSXPCConnection.current() {
            lock.lock()
            pushTargets.insert(ObjectIdentifier(connection))
            endpoint = appEndpoint
            generation = appEndpoint == nil ? 0 : endpointGeneration
            lock.unlock()
        } else {
            log.error("fetchAppEndpoint called with no current connection")
            lock.lock()
            endpoint = appEndpoint
            generation = appEndpoint == nil ? 0 : endpointGeneration
            lock.unlock()
        }

        if endpoint == nil {
            // Routine: Finder starts the extension at login, usually well before the app runs.
            log.debug("Extension asked for an endpoint before the app published one")
        } else {
            log.info("Handed endpoint generation \(generation, privacy: .public) to extension")
        }

        reply(endpoint, generation)
    }

    func brokerVersion(reply: @escaping (String) -> Void) {
        reply(brokerVersion)
    }

    func ping(reply: @escaping () -> Void) {
        reply()
    }

    // MARK: - Private

    /// Drop a connection, and with it the endpoint if that connection was the app's.
    ///
    /// Clearing the endpoint is what keeps the extension converging. An endpoint outlives the
    /// connection that carried it but dies with the listener that created it, so once the app
    /// is gone the stored endpoint is a dud. Continuing to hand it out would leave every
    /// extension reconnecting to a listener that no longer exists.
    private func forget(_ id: ObjectIdentifier, reason: String) {
        lock.lock()

        let wasKnown = connections.removeValue(forKey: id) != nil
        pushTargets.remove(id)

        var targets: [NSXPCConnection] = []
        var generation: UInt64 = 0
        let wasApp = appConnectionID == id

        if wasApp {
            appEndpoint = nil
            appConnectionID = nil
            endpointGeneration += 1
            generation = endpointGeneration
            targets = pushTargets.compactMap { connections[$0] }
        }

        let remaining = connections.count
        lock.unlock()

        guard wasKnown else { return }

        if wasApp {
            log.info("App connection \(reason, privacy: .public); cleared endpoint, generation \(generation, privacy: .public); \(remaining, privacy: .public) connection(s) left")
            push(endpoint: nil, generation: generation, to: targets)
        } else {
            log.info("Connection \(reason, privacy: .public); \(remaining, privacy: .public) connection(s) left")
        }
    }

    private func push(endpoint: NSXPCListenerEndpoint?,
                      generation: UInt64,
                      to targets: [NSXPCConnection]) {
        for connection in targets {
            let proxy = connection.remoteObjectProxyWithErrorHandler { [weak self] error in
                // A push failing is not fatal: the extension polls on its own backoff and will
                // pick the endpoint up. Worth logging because a persistent failure here means
                // the extension is not exporting the client protocol.
                self?.log.error("Failed to push endpoint to extension: \(error.localizedDescription, privacy: .public)")
            } as? FinderSyncBrokerClientProtocol

            proxy?.appEndpointDidChange(endpoint, generation: generation)
        }
    }
}

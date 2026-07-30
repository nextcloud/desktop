/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import Foundation
import os

///
/// Bring-up for the FinderSync broker login item.
///
/// Launched by launchd, either at login or on demand when the app or the FinderSync extension
/// looks up its Mach service. The Mach service name is the broker's own bundle identifier:
/// that is a Service Management login item requirement, and it also happens to place the name
/// inside the shared App Group prefix, which is what lets both sandboxed peers look it up with
/// no `temporary-exception` entitlement.
///
/// Everything lives inside `runBroker()` rather than in top-level code on purpose. Under the
/// Swift 6 language mode top-level code is `@MainActor`-isolated, which would make every
/// variable declared here a main-actor-isolated global and forbid the background reachability
/// probe from reading them. Locals in a plain function have no such constraint.
///

private let log = Logger(subsystem: Bundle.main.bundleIdentifier ?? "FinderSyncBroker",
                         category: "Broker")

/// Values templated into the bundle by the build system, so nothing here is branding-specific.
private struct BundleIdentity {
    let serviceName: String
    let teamIdentifier: String
    let applicationRevDomain: String
    let version: String
}

private func readBundleIdentity() -> BundleIdentity? {
    let info = Bundle.main.infoDictionary ?? [:]

    guard let serviceName = Bundle.main.bundleIdentifier, !serviceName.isEmpty else {
        log.critical("No bundle identifier; cannot determine the Mach service name to vend")
        return nil
    }
    guard let team = info["NCDevelopmentTeam"] as? String, !team.isEmpty else {
        log.critical("Info.plist is missing NCDevelopmentTeam; refusing to run without a peer requirement")
        return nil
    }
    guard let revDomain = info["NCApplicationRevDomain"] as? String, !revDomain.isEmpty else {
        log.critical("Info.plist is missing NCApplicationRevDomain; refusing to run without a peer requirement")
        return nil
    }

    return BundleIdentity(serviceName: serviceName,
                          teamIdentifier: team,
                          applicationRevDomain: revDomain,
                          version: info["CFBundleVersion"] as? String ?? "0")
}

/// Prove the listener is actually reachable, rather than assuming it is.
///
/// This is the check whose absence let 34.0.0 ship with a completely dead channel. Neither
/// `NSXPCListener.init(machServiceName:)` — which never returns nil — nor `resume()`, which
/// returns Void, can report that launchd refused to register the name; libxpc logs
/// "listener failed to activate" to its own subsystem and nothing else notices. So do not
/// infer success from those calls. Connect back to our own name and require a real reply:
/// that exercises launchd registration, the listener delegate, and the exported object in one
/// go, and it is the only evidence worth logging.
private func verifyListenerIsReachable(serviceName: String) {
    let probe = NSXPCConnection(machServiceName: serviceName, options: [])
    probe.remoteObjectInterface = NSXPCInterface(with: FinderSyncBrokerProtocol.self)
    probe.resume()

    let answered = DispatchSemaphore(value: 0)

    let proxy = probe.remoteObjectProxyWithErrorHandler { error in
        log.critical("""
            Self-check failed: the Mach service \(serviceName, privacy: .public) is not reachable \
            (\(error.localizedDescription, privacy: .public)). FinderSync badges and menus will \
            not work. Verify with: launchctl print gui/$(id -u)/\(serviceName, privacy: .public)
            """)
        answered.signal()
    } as? FinderSyncBrokerProtocol

    guard let proxy else {
        log.critical("Self-check failed: could not obtain a proxy for \(serviceName, privacy: .public)")
        probe.invalidate()
        return
    }

    proxy.ping {
        log.info("Self-check passed: \(serviceName, privacy: .public) is registered and serving")
        answered.signal()
    }

    if answered.wait(timeout: .now() + 5.0) == .timedOut {
        log.critical("""
            Self-check timed out after 5s: \(serviceName, privacy: .public) did not answer its own \
            ping. Treat the broker as not serving.
            """)
    }

    probe.invalidate()
}

private func runBroker() {
    guard let identity = readBundleIdentity() else {
        // Exiting non-zero makes launchd relaunch us, which would spin. A misconfigured bundle
        // is not transient, so exit cleanly and leave the diagnosis in the log.
        exit(EXIT_SUCCESS)
    }

    #if DEBUG
    let allowAppleDevelopment = true
    log.warning("""
        DEBUG BUILD: also accepting Apple Development signatures. Never ship this — the \
        production requirement rejects locally signed peers by design.
        """)
    #else
    let allowAppleDevelopment = false
    #endif

    let requirement = PeerRequirement.text(
        teamIdentifier: identity.teamIdentifier,
        identifiers: [
            identity.applicationRevDomain,                    // the desktop client
            "\(identity.applicationRevDomain).FinderSyncExt",  // the FinderSync extension
            identity.serviceName                               // ourselves, for the self-check
        ],
        allowAppleDevelopment: allowAppleDevelopment
    )

    let broker = Broker(log: log, brokerVersion: identity.version)

    let listener = NSXPCListener(machServiceName: identity.serviceName)
    listener.delegate = broker
    listener.setConnectionCodeSigningRequirement(requirement)
    listener.resume()

    log.info("Listener resumed for \(identity.serviceName, privacy: .public), version \(identity.version, privacy: .public); verifying reachability")

    // Off the main thread: the probe blocks on a reply the listener itself has to serve.
    let serviceName = identity.serviceName
    DispatchQueue.global(qos: .userInitiated).async {
        verifyListenerIsReachable(serviceName: serviceName)
    }

    // The listener and broker must outlive this function; the run loop never returns, so
    // holding them here is enough to keep them alive for the lifetime of the process.
    withExtendedLifetime((listener, broker)) {
        RunLoop.main.run()
    }
}

runBroker()

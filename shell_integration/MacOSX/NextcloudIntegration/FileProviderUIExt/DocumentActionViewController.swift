//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProviderUI
import FileProvider
import NCDesktopClientSocketKit
import NextcloudKit
import NextcloudFileProviderKit
import OSLog

class DocumentActionViewController: FPUIActionExtensionViewController {
    var domain: NSFileProviderDomain {
        guard let identifier = extensionContext.domainIdentifier else {
            fatalError("not expected to be called with default domain")
        }
        return NSFileProviderDomain(
            identifier: NSFileProviderDomainIdentifier(rawValue: identifier.rawValue),
            displayName: ""
        )
    }

    ///
    /// To be passed down in the hierarchy to all subordinate code.
    ///
    var log: (any FileProviderLogging)!

    ///
    /// To be used by this view controller only.
    ///
    /// Child view controllers must set up their own for clarity.
    ///
    var logger: FileProviderLogger!

    var serviceResolver: ServiceResolver!
    
    var socketClient: LocalSocketClient?
    lazy var lineProcessor: LineProcessor = NoOpLineProcessor()

    // MARK: - Lifecycle

    func setUp() {
        if log == nil {
            log = FileProviderLog(fileProviderDomainIdentifier: domain.identifier)
        }

        if logger == nil, let log {
            logger = FileProviderLogger(category: "DocumentActionViewController", log: log)
        }

        if serviceResolver == nil, let log {
            serviceResolver = ServiceResolver(log: log)
        }
        
        setupSocketClientForSocketApi()
    }
    
    private func setupSocketClientForSocketApi() {
        guard let socketApiPrefix = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix") as? String, !socketApiPrefix.isEmpty else {
            logger?.error("No SocketApiPrefix found")
            return
        }
        
        guard let containerUrl = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: socketApiPrefix) else {
            logger?.error("No containerURL available")
            return
        }

        let socketPath = containerUrl.appendingPathComponent("s", isDirectory: true)
        let processor = NoOpLineProcessor()
// FIXME
//        self.socketClient = LocalSocketClient(socketPath: socketPath.path, lineProcessor: processor)
//        self.socketClient?.start()
//        self.logger?.debug("Socket client started successfully")
    }

    func prepare(childViewController: NSViewController) {
        setUp()
        addChild(childViewController)
        view.addSubview(childViewController.view)

        NSLayoutConstraint.activate([
            view.leadingAnchor.constraint(equalTo: childViewController.view.leadingAnchor),
            view.trailingAnchor.constraint(equalTo: childViewController.view.trailingAnchor),
            view.topAnchor.constraint(equalTo: childViewController.view.topAnchor),
            view.bottomAnchor.constraint(equalTo: childViewController.view.bottomAnchor)
        ])
    }

    override func prepare(forAction actionIdentifier: String, itemIdentifiers: [NSFileProviderItemIdentifier]) {
        setUp()
        logger?.info("Preparing action: \(actionIdentifier)")

        switch (actionIdentifier) {
            case "com.nextcloud.desktopclient.FileProviderUIExt.ShareAction":
                prepare(childViewController: ShareViewController(itemIdentifiers, serviceResolver: serviceResolver, log: log))
            case "com.nextcloud.desktopclient.FileProviderUIExt.LockFileAction":
                prepare(childViewController: LockViewController(itemIdentifiers, locking: true, serviceResolver: serviceResolver, log: log))
            case "com.nextcloud.desktopclient.FileProviderUIExt.UnlockFileAction":
                prepare(childViewController: LockViewController(itemIdentifiers, locking: false, serviceResolver: serviceResolver, log: log))
            case "com.nextcloud.desktopclient.FileProviderUIExt.EvictAction":
                evict(itemsWithIdentifiers: itemIdentifiers, inDomain: domain);
                extensionContext.completeRequest();
            case "com.nextcloud.desktopclient.FileProviderUIExt.FileActionsAction":
                showFileActions(itemsWithIdentifiers: itemIdentifiers, inDomain: domain);
            default:
                return
        }
    }

    override func prepare(forError error: Error) {
        setUp()
        logger?.info("Preparing for error.", [.error: error])

        let storyboard = NSStoryboard(name: "Authentication", bundle: Bundle(for: type(of: self)))
        let viewController = storyboard.instantiateInitialController() as! AuthenticationViewController
        viewController.log = log
        viewController.serviceResolver = serviceResolver

        prepare(childViewController: viewController)
    }

    override public func loadView() {
        self.view = NSView()
    }

    // MARK: - Eviction

    ///
    /// Use a file provider domain manager to evict all items identified by the given array.
    ///
    func evict(itemsWithIdentifiers identifiers: [NSFileProviderItemIdentifier], inDomain domain: NSFileProviderDomain) async {
        logger?.debug("Starting eviction process…")

        guard let manager = NSFileProviderManager(for: domain) else {
            logger?.error("Could not get file provider domain manager.", [.domain: domain.identifier])
            return;
        }
        do {
            for itemIdentifier in identifiers {
                logger?.error("Evicting item: \(itemIdentifier.rawValue)")
                try await manager.evictItem(identifier: itemIdentifier)
            }
        } catch let error {
            logger?.error("Error evicting item: \(error.localizedDescription)")
        }
    }

    ///
    /// Synchronous wrapper of ``evict(itemsWithIdentifiers:inDomain:)-67w8c``.
    ///
    func evict(itemsWithIdentifiers identifiers: [NSFileProviderItemIdentifier], inDomain domain: NSFileProviderDomain) {
        let semaphore = DispatchSemaphore(value: 0)

        Task {
            await evict(itemsWithIdentifiers: identifiers, inDomain: domain)
            semaphore.signal()
        }

        semaphore.wait()
    }
    
    func showFileActions(itemsWithIdentifiers identifiers: [NSFileProviderItemIdentifier], inDomain domain: NSFileProviderDomain) {
        guard let firstIdentifier = identifiers.first else {
            logger?.error("No item identifiers provided.")
            extensionContext.completeRequest()
            return
        }

        Task {
            do {
                guard let manager = NSFileProviderManager(for: domain) else {
                    logger?.error("Could not get file provider domain manager.")
                    await MainActor.run { extensionContext.completeRequest() }
                    return
                }
                let itemURL = try await manager.getUserVisibleURL(for: firstIdentifier)
                let localPath = itemURL.path
                let command = "FILE_ACTIONS:\(localPath)\n"
// FIXME
//                self.socketClient?.sendMessage(command)
//                self.logger?.debug("FILE_ACTIONS sent for: \(localPath)")
                await MainActor.run { extensionContext.completeRequest() }
            } catch {
                logger?.error("Error: \(error.localizedDescription)")
                await MainActor.run { extensionContext.completeRequest() }
            }
        }
    }
}

private final class NoOpLineProcessor: NSObject, LineProcessor {
    func process(_ line: String) {
    }
}


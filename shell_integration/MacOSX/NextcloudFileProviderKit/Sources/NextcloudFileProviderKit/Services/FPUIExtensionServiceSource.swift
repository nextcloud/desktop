//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudKit
import OSLog

class FPUIExtensionServiceSource: NSObject, NSFileProviderServiceSource, NSXPCListenerDelegate, FPUIExtensionService {
    let keychain: Keychain
    let listener = NSXPCListener.anonymous()
    let logger: FileProviderLogger
    let serviceName = fpUiExtensionServiceName
    let fpExtension: FileProviderExtension

    init(fpExtension: FileProviderExtension) {
        keychain = Keychain(log: fpExtension.log)
        logger = FileProviderLogger(category: "FPUIExtensionServiceSource", log: fpExtension.log)
        logger.debug("Instantiating FPUIExtensionService service")
        self.fpExtension = fpExtension
        super.init()
    }

    func makeListenerEndpoint() throws -> NSXPCListenerEndpoint {
        listener.delegate = self
        listener.resume()
        return listener.endpoint
    }

    func listener(
        _: NSXPCListener,
        shouldAcceptNewConnection newConnection: NSXPCConnection
    ) -> Bool {
        newConnection.exportedInterface = NSXPCInterface(with: FPUIExtensionService.self)
        newConnection.exportedObject = self
        newConnection.resume()
        return true
    }

    // MARK: - FPUIExtensionService protocol methods

    func authenticate() async -> NSError? {
        logger.info("Authenticating...")

        guard let user = fpExtension.config.user, let userId = fpExtension.config.userId, let serverUrl = fpExtension.config.serverUrl, let password = keychain.getPassword(for: user, on: serverUrl) else {
            logger.error("Missing account information, cannot authenticate!")
            return NSError(.missingAccountInformation)
        }

        return await withCheckedContinuation { continuation in
            fpExtension.setupDomainAccount(user: user, userId: userId, serverUrl: serverUrl, password: password) { error in
                continuation.resume(returning: error)
            }
        }
    }

    func userAgent() async -> NSString? {
        guard let account = fpExtension.ncAccount?.ncKitAccount else {
            return nil
        }

        let nkSession = fpExtension.ncKit.nkCommonInstance.nksessions.session(forAccount: account)
        return nkSession?.userAgent as NSString?
    }

    func credentials() async -> NSDictionary {
        (fpExtension.ncAccount?.dictionary() ?? [:]) as NSDictionary
    }

    func itemServerPath(identifier: NSFileProviderItemIdentifier) async -> NSString? {
        let rawIdentifier = identifier.rawValue
        logger.info("Fetching shares for item \(rawIdentifier)")

        guard let baseUrl = fpExtension.ncAccount?.davFilesUrl else {
            logger.error("Could not fetch shares as ncAccount on parent extension is nil")
            return nil
        }

        guard let account = fpExtension.ncAccount?.ncKitAccount else {
            logger.error("Could not fetch ncKitAccount on parent extension")
            return nil
        }
        guard let dbManager = fpExtension.dbManager else {
            logger.error("Could not get db manager for \(account)")
            return nil
        }
        guard let item = dbManager.itemMetadata(identifier) else {
            logger.error("No item \(rawIdentifier) in db, no shares.")
            return nil
        }

        let completePath = item.serverUrl + "/" + item.fileName
        return completePath.replacingOccurrences(of: baseUrl, with: "") as NSString
    }

    func presentUnifiedSharingDialog(identifier: NSFileProviderItemIdentifier, localPath: NSString) async -> Bool {
        guard let account = fpExtension.ncAccount else {
            logger.error("Could not present unified sharing because the account is unavailable.", [.item: identifier])
            return false
        }

        guard let dbManager = fpExtension.dbManager,
              let metadata = dbManager.itemMetadata(identifier),
              !metadata.fileId.isEmpty
        else {
            logger.error("Could not present unified sharing because the item metadata or numeric file id is unavailable.", [.item: identifier])
            return false
        }

        let (_, _, responseData, error) = await fpExtension.ncKit.fetchCapabilities(account: account)
        guard error == .success else {
            logger.error("Could not determine whether unified sharing is supported.", [.item: identifier, .error: error])
            return false
        }

        guard UnifiedSharingCapability.isAvailable(in: responseData) else {
            logger.info("Unified sharing is not supported; using the legacy sharing interface.", [.item: identifier])
            return false
        }

        guard let app = fpExtension.app else {
            logger.error("Could not present unified sharing because the main app connection is unavailable.", [.item: identifier])
            return false
        }

        let domainIdentifier = fpExtension.domain.identifier.rawValue
        app.presentUnifiedSharing(
            forItem: metadata.fileId,
            localPath: localPath as String,
            remoteItemPath: metadata.path,
            forDomainIdentifier: domainIdentifier
        )
        logger.info("Asked the main app to present unified sharing.", [.item: identifier, .domain: fpExtension.domain.identifier])
        return true
    }
}

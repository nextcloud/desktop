//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit
import OSLog

class FPUIExtensionServiceSource: NSObject, NSFileProviderServiceSource, NSXPCListenerDelegate, FPUIExtensionService {
    let listener = NSXPCListener.anonymous()
    let logger: FileProviderLogger
    let serviceName = fpUiExtensionServiceName
    let fpExtension: FileProviderExtension

    init(fpExtension: FileProviderExtension) {
        logger = FileProviderLogger(category: "FPUIExtensionServiceSource", log: fpExtension.log)
        logger.debug("Instantiating service source.")
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
        logger.info("Authenticating.")

        guard let account = fpExtension.storedAccountForAuthentication() else {
            logger.error("Cannot authenticate because account information is missing.", [.domain: fpExtension.domain.identifier.rawValue])
            return NSError(.missingAccountInformation)
        }

        return await withCheckedContinuation { continuation in
            fpExtension.setupDomainAccount(
                user: account.username,
                userId: account.id,
                serverUrl: account.serverUrl,
                password: account.password
            ) { error in
                continuation.resume(returning: error)
            }
        }
    }

    func userAgent() async -> NSString? {
        guard let (dbManager, operation) = try? fpExtension.beginSyncOperation() else {
            return nil
        }
        defer { operation.finish() }

        let nkSession = fpExtension.ncKit.nkCommonInstance.nksessions.session(forAccount: dbManager.account.ncKitAccount)
        guard operation.beginDelivery() else { return nil }
        return nkSession?.userAgent as NSString?
    }

    func credentials() async -> NSDictionary {
        guard let (dbManager, operation) = try? fpExtension.beginSyncOperation() else {
            return [:]
        }
        defer { operation.finish() }
        guard operation.beginDelivery() else { return [:] }
        return dbManager.account.dictionary() as NSDictionary
    }

    func itemServerPath(identifier: NSFileProviderItemIdentifier) async -> NSString? {
        let rawIdentifier = identifier.rawValue
        logger.info("Fetching shares for item \(rawIdentifier)")

        guard let (dbManager, operation) = try? fpExtension.beginSyncOperation() else {
            logger.error("Could not fetch the item path because the account is unavailable.", [.item: identifier])
            return nil
        }
        defer { operation.finish() }
        guard let item = dbManager.itemMetadata(identifier) else {
            logger.error("No item \(rawIdentifier) in db, no shares.")
            return nil
        }

        guard operation.beginDelivery() else { return nil }
        let completePath = item.serverUrl + "/" + item.fileName
        return completePath.replacingOccurrences(of: dbManager.account.davFilesUrl, with: "") as NSString
    }

    func presentUnifiedSharingDialog(identifier: NSFileProviderItemIdentifier, localPath: NSString) async -> Bool {
        guard let (dbManager, operation) = try? fpExtension.beginSyncOperation() else {
            logger.error("Could not present unified sharing because the account is unavailable.", [.item: identifier])
            return false
        }
        defer { operation.finish() }

        guard let metadata = dbManager.itemMetadata(identifier),
              !metadata.fileId.isEmpty
        else {
            logger.error("Could not present unified sharing because the item metadata or numeric file id is unavailable.", [.item: identifier])
            return false
        }

        let (_, capabilities, _, error) = await fpExtension.ncKit.fetchCapabilities(
            account: dbManager.account,
            taskHandler: { operation.registerReadTask($0) }
        )
        guard error == .success else {
            logger.error("Could not determine whether unified sharing is supported.", [.item: identifier, .error: error])
            return false
        }

        guard capabilities?.sharing != nil else {
            logger.info("Unified sharing is not supported; using the legacy sharing interface.", [.item: identifier])
            return false
        }

        guard let app = fpExtension.app else {
            logger.error("Could not present unified sharing because the main app connection is unavailable.", [.item: identifier])
            return false
        }
        guard operation.beginDelivery() else { return false }

        let domainIdentifier = fpExtension.domain.identifier.rawValue
        app.presentUnifiedSharing(
            forItem: metadata.fileId,
            localPath: localPath as String,
            remoteItemPath: metadata.davParentPathWithoutOrigin,
            forDomainIdentifier: domainIdentifier
        )
        logger.info("Asked the main app to present unified sharing.", [.item: identifier, .domain: fpExtension.domain.identifier])
        return true
    }
}

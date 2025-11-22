//
//  FPUIExtensionCommunicationService.swift
//  FileProviderExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import FileProvider
import Foundation
import NextcloudKit
import NextcloudFileProviderKit
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
        _ listener: NSXPCListener,
        shouldAcceptNewConnection newConnection: NSXPCConnection
    ) -> Bool {
        newConnection.exportedInterface = NSXPCInterface(with: FPUIExtensionService.self)
        newConnection.exportedObject = self
        newConnection.resume()
        return true
    }

    //MARK: - FPUIExtensionService protocol methods

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
        return (fpExtension.ncAccount?.dictionary() ?? [:]) as NSDictionary
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
}

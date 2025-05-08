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
    let listener = NSXPCListener.anonymous()
    let serviceName = fpUiExtensionServiceName
    let fpExtension: FileProviderExtension

    init(fpExtension: FileProviderExtension) {
        Logger.fpUiExtensionService.debug("Instantiating FPUIExtensionService service")
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

    func userAgent() async -> NSString? {
        guard let account = fpExtension.ncAccount?.ncKitAccount else {
            return nil
        }
        let nkSession = fpExtension.ncKit.getSession(account: account)
        return nkSession?.userAgent as NSString?
    }

    func credentials() async -> NSDictionary {
        return (fpExtension.ncAccount?.dictionary() ?? [:]) as NSDictionary
    }

    func itemServerPath(identifier: NSFileProviderItemIdentifier) async -> NSString? {
        let rawIdentifier = identifier.rawValue
        Logger.shares.info("Fetching shares for item \(rawIdentifier, privacy: .public)")

        guard let baseUrl = fpExtension.ncAccount?.davFilesUrl else {
            Logger.shares.error("Could not fetch shares as ncAccount on parent extension is nil")
            return nil
        }

        guard let account = fpExtension.ncAccount?.ncKitAccount else {
            Logger.shares.error("Could not fetch ncKitAccount on parent extension")
            return nil
        }
        guard let dbManager = fpExtension.dbManager else {
            Logger.shares.error("Could not get db manager for \(account, privacy: .public)")
            return nil
        }
        guard let item = dbManager.itemMetadataFromFileProviderItemIdentifier(identifier) else {
            Logger.shares.error("No item \(rawIdentifier, privacy: .public) in db, no shares.")
            return nil
        }

        let completePath = item.serverUrl + "/" + item.fileName
        return completePath.replacingOccurrences(of: baseUrl, with: "") as NSString
    }
}

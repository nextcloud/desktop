//
//  FPUIExtensionCommunicationService.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 21/2/24.
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

        let dbManager = FilesDatabaseManager.shared
        guard let item = dbManager.itemMetadataFromFileProviderItemIdentifier(identifier) else {
            Logger.shares.error("No item \(rawIdentifier, privacy: .public) in db, no shares.")
            return nil
        }

        let completePath = item.serverUrl + "/" + item.fileName
        return completePath.replacingOccurrences(of: baseUrl, with: "") as NSString
    }
}

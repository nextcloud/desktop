//
//  FPUIExtensionCommunicationService.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 21/2/24.
//

import FileProvider
import Foundation
import NextcloudKit
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

    func shares(
        forItemIdentifier itemIdentifier: NSFileProviderItemIdentifier
    ) async -> [NKShare]? {
        let controller = ItemSharesController(
            itemIdentifier: itemIdentifier, parentExtension: fpExtension
        )
        return await controller.fetch()
    }

    func credentials() async -> NSDictionary {
        return (fpExtension.ncAccount?.dictionary() ?? [:]) as NSDictionary
    }
}

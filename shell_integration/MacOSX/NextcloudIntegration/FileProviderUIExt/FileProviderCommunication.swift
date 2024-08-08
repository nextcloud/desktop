//
//  FileProviderCommunication.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 30/7/24.
//

import FileProvider

enum FileProviderCommunicationError: Error {
    case serviceNotFound
    case remoteProxyObjectInvalid
}

func serviceConnection(
    url: URL, interruptionHandler: @escaping () -> Void
) async throws -> FPUIExtensionService {
    let services = try await FileManager().fileProviderServicesForItem(at: url)
    guard let service = services[fpUiExtensionServiceName] else {
        throw FileProviderCommunicationError.serviceNotFound
    }
    let connection: NSXPCConnection
    connection = try await service.fileProviderConnection()
    connection.remoteObjectInterface = NSXPCInterface(with: FPUIExtensionService.self)
    connection.interruptionHandler = interruptionHandler
    connection.resume()
    guard let proxy = connection.remoteObjectProxy as? FPUIExtensionService else {
        throw FileProviderCommunicationError.remoteProxyObjectInvalid
    }
    return proxy
}

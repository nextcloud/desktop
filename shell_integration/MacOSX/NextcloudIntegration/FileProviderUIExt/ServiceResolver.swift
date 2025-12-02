//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudFileProviderKit

///
/// Facility to establish and handle an XPC connection to the file provider extension.
///
final class ServiceResolver {
    enum ServiceResolverError: Error {
        case failedConnection
        case remoteProxyObjectInvalid
        case serviceNotFound
    }

    let log: any FileProviderLogging
    let logger: FileProviderLogger

    init(log: any FileProviderLogging) {
        self.log = log
        self.logger = FileProviderLogger(category: "ServiceResolver", log: log)
    }

    ///
    /// Logs the interruption of a connection.
    ///
    private func interruptionHandler() {
        logger.error("Interruption handler called. Possibly, the remote file provider extension process exited or crashed.")
    }

    ///
    /// Logs the invalidation of a connection.
    ///
    private func invalidationHandler() {
        logger.error("Invalidation handler called. Possibly, the remote file provider extension process exited or crashed.")
    }

    func getService(at url: URL) async throws -> FPUIExtensionService {
        logger.info("Getting service for item at location.", [.url: url])

        var services: [NSFileProviderServiceName : NSFileProviderService] = [:]

        do {
            if url.startAccessingSecurityScopedResource() {
                logger.debug("Started accessing security-scoped resource.", [.url: url])
                services = try await FileManager().fileProviderServicesForItem(at: url)
                url.stopAccessingSecurityScopedResource()
                logger.debug("Stopped accessing security-scoped resource.", [.url: url])
            } else {
                logger.error("Failed to access security-scoped resource!", [.url: url])
            }
        } catch {
            logger.error("Failed to get file provider services for item!", [.url: url])
            throw error
        }

        guard let service = services[fpUiExtensionServiceName] else {
            logger.error("Failed to find service by name in array of returned services!", [.name: fpUiExtensionServiceName])
            throw ServiceResolverError.serviceNotFound
        }

        let connection: NSXPCConnection?

        do {
            connection = try await service.fileProviderConnection()
        } catch {
            logger.error("Failed to establish XPC connection!")
            throw ServiceResolverError.failedConnection
        }

        guard let connection else {
            throw ServiceResolverError.failedConnection
        }

        connection.remoteObjectInterface = NSXPCInterface(with: FPUIExtensionService.self)
        connection.interruptionHandler = interruptionHandler
        connection.invalidationHandler = invalidationHandler
        connection.resume()

        guard let proxy = connection.remoteObjectProxy as? FPUIExtensionService else {
            logger.error("The remote object proxy does not conform to the expected protocol!")
            throw ServiceResolverError.remoteProxyObjectInvalid
        }

        logger.info("Providing service.")

        return proxy
    }
}

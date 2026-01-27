//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

extension FileProviderExtension: NSXPCListenerDelegate {
    func listener(_ listener: NSXPCListener, shouldAcceptNewConnection newConnection: NSXPCConnection) -> Bool {
        logger.info("Listener should accept new connection...")

        newConnection.exportedInterface = NSXPCInterface(with: ClientCommunicationProtocol.self)
        newConnection.exportedObject = self
        newConnection.remoteObjectInterface = NSXPCInterface(with: AppProtocol.self)

        newConnection.interruptionHandler = {
            self.logger.info("XPC connection interrupted!")
            self.connections.remove(newConnection)
        }

        newConnection.invalidationHandler = {
            self.logger.info("XPC connection invalidated!")
            self.connections.remove(newConnection)
        }

        connections.insert(newConnection)
        newConnection.resume()

        let remoteObjectProxy = newConnection.remoteObjectProxyWithErrorHandler { error in
            self.logger.error("Error while fetching remote object proxy: \(error)")
        }

        if let appService = remoteObjectProxy as? AppProtocol {
            logger.info("Succeeded to cast remote object proxy, adopting it!")
            self.app = appService
        } else {
            logger.error("Failed to cast remote object proxy to AppProtocol!")
            self.app = nil
        }

        return true
    }
}

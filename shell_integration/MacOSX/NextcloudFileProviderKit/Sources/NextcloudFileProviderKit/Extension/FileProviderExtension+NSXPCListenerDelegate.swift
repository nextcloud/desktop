//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudFileProviderXPC

extension FileProviderExtension: NSXPCListenerDelegate {
    public func listener(_: NSXPCListener, shouldAcceptNewConnection newConnection: NSXPCConnection) -> Bool {
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
            app = appService

            // Force a fresh status report as soon as the app service is available. The app needs
            // the current state on launch (typically idle, i.e. "all good"); without it the tray
            // popover falls back to the misleading "Some files could not be synced!" message.
            // This must NOT go through `updatedSyncStateReporting(oldActions:)`: that is
            // edge-triggered and bails when sync activity has not changed, which is precisely the
            // idle case we need to report here. See https://github.com/nextcloud/desktop/issues/10053.
            reportCurrentSyncState()
        } else {
            logger.error("Failed to cast remote object proxy to AppProtocol!")
            app = nil
        }

        return true
    }
}

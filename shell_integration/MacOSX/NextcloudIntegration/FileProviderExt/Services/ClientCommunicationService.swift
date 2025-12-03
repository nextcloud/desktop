/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import Foundation
import FileProvider
import NextcloudFileProviderKit
import OSLog

class ClientCommunicationService: NSObject, NSFileProviderServiceSource, NSXPCListenerDelegate, ClientCommunicationProtocol {
    let listener = NSXPCListener.anonymous()
    let logger: FileProviderLogger
    let serviceName = NSFileProviderServiceName("com.nextcloud.desktopclient.ClientCommunicationService")
    let fpExtension: FileProviderExtension
    private var mainAppConnection: NSXPCConnection?

    init(fpExtension: FileProviderExtension) {
        self.logger = FileProviderLogger(category: "ClientCommunicationService", log: fpExtension.log)
        logger.debug("Instantiating client communication service")
        self.fpExtension = fpExtension
        super.init()
    }

    func makeListenerEndpoint() throws -> NSXPCListenerEndpoint {
        listener.delegate = self
        listener.resume()

        return listener.endpoint
    }

    func listener(_ listener: NSXPCListener, shouldAcceptNewConnection newConnection: NSXPCConnection) -> Bool {
        newConnection.exportedInterface = NSXPCInterface(with: ClientCommunicationProtocol.self)
        newConnection.exportedObject = self
        newConnection.remoteObjectInterface = NSXPCInterface(with: MainAppServiceProtocol.self)
        self.mainAppConnection = newConnection
        newConnection.resume()

        return true
    }

    //MARK: - Client Communication Protocol methods

    func configureAccount(
        withUser user: String,
        userId: String,
        serverUrl: String,
        password: String,
        userAgent: String
    ) {
        logger.info("Received configure account information over client communication service")
        self.fpExtension.setupDomainAccount(
            user: user,
            userId: userId,
            serverUrl: serverUrl,
            password: password,
            userAgent: userAgent
        )
    }

    func removeAccountConfig() {
        self.fpExtension.removeAccountConfig()
    }

    func getTrashDeletionEnabledState(completionHandler: @escaping (Bool, Bool) -> Void) {
        let enabled = fpExtension.config.trashDeletionEnabled
        let set = fpExtension.config.trashDeletionSet
        completionHandler(enabled, set)
    }

    func setTrashDeletionEnabled(_ enabled: Bool) {
        fpExtension.config.trashDeletionEnabled = enabled
        logger.info("Trash deletion setting changed to: \(enabled)")
    }

    func setIgnoreList(_ ignoreList: [String]) {
        self.fpExtension.ignoredFiles = IgnoredFilesMatcher(ignoreList: ignoreList)
        logger.info("Ignore list updated.")
    }

    // MARK: - Report status to main app

    func reportStatusToMainApp(status: String) {
        guard let connection = self.mainAppConnection else {
            logger.error("Main app connection not available, cannot report status!")
            return
        }

        guard let remote = connection.remoteObjectProxy as? MainAppServiceProtocol else {
            logger.error("Remote object proxy does not conform to MainAppServiceProtocol!")
            return
        }

            let domainId = self.fpExtension.domain.identifier
            remote.reportStatus(forDomain: domainId, status: status)
            logger.debug("Reported status to main app", [.domain: domainId.rawValue, .name: status])
    }
}

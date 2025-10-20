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
        newConnection.resume()
        return true
    }

    //MARK: - Client Communication Protocol methods

    func getFileProviderDomainIdentifier(completionHandler: @escaping (String?, Error?) -> Void) {
        let identifier = self.fpExtension.domain.identifier.rawValue
        logger.debug("Returning file provider domain identifier.", [.domain: identifier])
        completionHandler(identifier, nil)
    }

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
}

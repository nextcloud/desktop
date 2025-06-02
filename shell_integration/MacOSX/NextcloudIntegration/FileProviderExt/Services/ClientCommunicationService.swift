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
    let serviceName = NSFileProviderServiceName("com.nextcloud.desktopclient.ClientCommunicationService")
    let fpExtension: FileProviderExtension

    init(fpExtension: FileProviderExtension) {
        Logger.desktopClientConnection.debug("Instantiating client communication service")
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

    func getExtensionAccountId(completionHandler: @escaping (String?, Error?) -> Void) {
        let accountUserId = self.fpExtension.domain.identifier.rawValue
        Logger.desktopClientConnection.info("Sending extension account ID \(accountUserId, privacy: .public)")
        completionHandler(accountUserId, nil)
    }

    func configureAccount(
        withUser user: String,
        userId: String,
        serverUrl: String,
        password: String,
        userAgent: String
    ) {
        Logger.desktopClientConnection.info("Received configure account information over client communication service")
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

    func createDebugLogString(completionHandler: ((String?, Error?) -> Void)!) {
        if #available(macOSApplicationExtension 12.0, *) {
            let (logs, error) = Logger.logEntries()
            guard error == nil else {
                Logger.logger.error("Cannot create debug archive, received error: \(error, privacy: .public)")
                completionHandler(nil, error)
                return
            }
            guard let logs = logs else {
                Logger.logger.error("Canot create debug archive with nil logs.")
                completionHandler(nil, nil)
                return
            }
            completionHandler(logs.joined(separator: "\n"), nil)
        }
    }

    func getTrashDeletionEnabledState(completionHandler: @escaping (Bool, Bool) -> Void) {
        let enabled = fpExtension.config.trashDeletionEnabled
        let set = fpExtension.config.trashDeletionSet
        completionHandler(enabled, set)
    }

    func setTrashDeletionEnabled(_ enabled: Bool) {
        fpExtension.config.trashDeletionEnabled = enabled
        Logger.fileProviderExtension.info(
            "Trash deletion setting changed to: \(enabled, privacy: .public)"
        )
    }

    func setIgnoreList(_ ignoreList: [String]) {
        self.fpExtension.ignoredFiles = IgnoredFilesMatcher(ignoreList: ignoreList)
        Logger.fileProviderExtension.info("Ignore list updated.")
    }
}

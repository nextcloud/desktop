/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import Foundation
import FileProvider
import OSLog
import NCDesktopClientSocketKit
import NextcloudKit

extension FileProviderExtension {
    func sendFileProviderDomainIdentifier() {
        let command = "FILE_PROVIDER_DOMAIN_IDENTIFIER_REQUEST_REPLY"
        let argument = domain.identifier.rawValue
        let message = command + ":" + argument + "\n"
        socketClient?.sendMessage(message)
    }

    private func signalEnumeratorAfterAccountSetup() {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error("Could not get file provider manager for domain \(self.domain.displayName, privacy: .public), cannot notify after account setup")
            return
        }

        assert(ncAccount != nil)

        fpManager.signalErrorResolved(NSFileProviderError(.notAuthenticated)) { error in
            if error != nil {
                Logger.fileProviderExtension.error("Error resolving not authenticated, received error: \(error!.localizedDescription)")
            }
        }

        Logger.fileProviderExtension.debug("Signalling enumerators for user \(self.ncAccount!.username) at server \(self.ncAccount!.serverUrl, privacy: .public)")

        fpManager.signalEnumerator(for: .workingSet) { error in
            if error != nil {
                Logger.fileProviderExtension.error("Error signalling enumerator for working set, received error: \(error!.localizedDescription, privacy: .public)")
            }
        }
    }

    func setupDomainAccount(user: String, serverUrl: String, password: String) {
        ncAccount = NextcloudAccount(user: user, serverUrl: serverUrl, password: password)
        ncKit.setup(user: ncAccount!.username,
                    userId: ncAccount!.username,
                    password: ncAccount!.password,
                    urlBase: ncAccount!.serverUrl,
                    userAgent: "Nextcloud-macOS/FileProviderExt",
                    nextcloudVersion: 25,
                    delegate: nil) // TODO: add delegate methods for self

        Logger.fileProviderExtension.info("Nextcloud account set up in File Provider extension for user: \(user, privacy: .public) at server: \(serverUrl, privacy: .public)")

        signalEnumeratorAfterAccountSetup()
    }

    func removeAccountConfig() {
        Logger.fileProviderExtension.info("Received instruction to remove account data for user \(self.ncAccount!.username, privacy: .public) at server \(self.ncAccount!.serverUrl, privacy: .public)")
        ncAccount = nil
    }
}

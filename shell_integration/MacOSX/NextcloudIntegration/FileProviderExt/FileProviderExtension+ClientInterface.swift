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

import FileProvider
import Foundation
import NCDesktopClientSocketKit
import NextcloudKit
import OSLog

extension FileProviderExtension: NSFileProviderServicing {
    /*
     This FileProviderExtension extension contains everything needed to communicate with the client.
     We have two systems for communicating between the extensions and the client.

     Apple's XPC based File Provider APIs let us easily communicate client -> extension.
     This is what ClientCommunicationService is for.

     We also use sockets, because the File Provider XPC system does not let us easily talk from
     extension->client.
     We need this because the extension needs to be able to request account details. We can't
     reliably do this via XPC because the extensions get torn down by the system, out of the control
     of the app, and we can receive nil/no services from NSFileProviderManager. Once this is done
     then XPC works ok.
    */
    func supportedServiceSources(
        for itemIdentifier: NSFileProviderItemIdentifier,
        completionHandler: @escaping ([NSFileProviderServiceSource]?, Error?) -> Void
    ) -> Progress {
        Logger.desktopClientConnection.debug("Serving supported service sources")
        let clientCommService = ClientCommunicationService(fpExtension: self)
        let services = [clientCommService]
        completionHandler(services, nil)
        let progress = Progress()
        progress.cancellationHandler = {
            let error = NSError(domain: NSCocoaErrorDomain, code: NSUserCancelledError)
            completionHandler(nil, error)
        }
        return progress
    }

    @objc func sendFileProviderDomainIdentifier() {
        let command = "FILE_PROVIDER_DOMAIN_IDENTIFIER_REQUEST_REPLY"
        let argument = domain.identifier.rawValue
        let message = command + ":" + argument + "\n"
        socketClient?.sendMessage(message)
    }

    private func signalEnumeratorAfterAccountSetup() {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error(
                "Could not get file provider manager for domain \(self.domain.displayName, privacy: .public), cannot notify after account setup"
            )
            return
        }

        assert(ncAccount != nil)

        fpManager.signalErrorResolved(NSFileProviderError(.notAuthenticated)) { error in
            if error != nil {
                Logger.fileProviderExtension.error(
                    "Error resolving not authenticated, received error: \(error!.localizedDescription)"
                )
            }
        }

        Logger.fileProviderExtension.debug(
            "Signalling enumerators for user \(self.ncAccount!.username) at server \(self.ncAccount!.serverUrl, privacy: .public)"
        )

        fpManager.signalEnumerator(for: .workingSet) { error in
            if error != nil {
                Logger.fileProviderExtension.error(
                    "Error signalling enumerator for working set, received error: \(error!.localizedDescription, privacy: .public)"
                )
            }
        }
    }

    @objc func setupDomainAccount(user: String, serverUrl: String, password: String) {
        let newNcAccount = NextcloudAccount(user: user, serverUrl: serverUrl, password: password)
        guard newNcAccount != ncAccount else { return }
        ncAccount = newNcAccount
        ncKit.setup(
            user: ncAccount!.username,
            userId: ncAccount!.username,
            password: ncAccount!.password,
            urlBase: ncAccount!.serverUrl,
            userAgent: "Nextcloud-macOS/FileProviderExt",
            nextcloudVersion: 25,
            delegate: nil) // TODO: add delegate methods for self

        Logger.fileProviderExtension.info(
            "Nextcloud account set up in File Provider extension for user: \(user, privacy: .public) at server: \(serverUrl, privacy: .public)"
        )

        signalEnumeratorAfterAccountSetup()
    }

    @objc func removeAccountConfig() {
        Logger.fileProviderExtension.info(
            "Received instruction to remove account data for user \(self.ncAccount!.username, privacy: .public) at server \(self.ncAccount!.serverUrl, privacy: .public)"
        )
        ncAccount = nil
    }
}

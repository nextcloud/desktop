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
import NextcloudFileProviderKit
import OSLog

let AuthenticationTimeouts: [UInt64] = [ // Have progressively longer timeouts to not hammer server
    3_000_000_000, 6_000_000_000, 30_000_000_000, 60_000_000_000, 120_000_000_000, 300_000_000_000
]

extension FileProviderExtension: NSFileProviderServicing, ChangeNotificationInterface {
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
        let fpuiExtService = FPUIExtensionServiceSource(fpExtension: self)
        let services: [NSFileProviderServiceSource] = [clientCommService, fpuiExtService]
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

        notifyChange()
    }

    func notifyChange() {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error(
                "Could not get file provider manager for domain \(self.domain.displayName, privacy: .public), cannot notify changes"
            )
            return
        }

        fpManager.signalEnumerator(for: .workingSet) { error in
            if error != nil {
                Logger.fileProviderExtension.error(
                    "Error signalling enumerator for working set, received error: \(error!.localizedDescription, privacy: .public)"
                )
            }
        }
    }

    @objc func setupDomainAccount(
        user: String, userId: String, serverUrl: String, password: String
    ) {
        let account = Account(user: user, id: userId, serverUrl: serverUrl, password: password)
        guard account != ncAccount else { return }

        Task {
            ncKit.appendSession(
                account: account.ncKitAccount,
                urlBase: serverUrl,
                user: user,
                userId: userId,
                password: password,
                userAgent: "Nextcloud-macOS/FileProviderExt",
                nextcloudVersion: 25,
                groupIdentifier: ""
            )
            var authAttemptState = AuthenticationAttemptResultState.connectionError // default

            // Retry a few times if we have a connection issue
            for authTimeout in AuthenticationTimeouts {
                authAttemptState = await ncKit.tryAuthenticationAttempt(account: account)
                guard authAttemptState == .connectionError else { break }

                Logger.fileProviderExtension.info(
                    "\(user, privacy: .public) authentication try timed out. Trying again soon."
                )
                try? await Task.sleep(nanoseconds: authTimeout)
            }

            switch (authAttemptState) {
            case .authenticationError:
                Logger.fileProviderExtension.info(
                    "\(user, privacy: .public) authentication failed due to bad creds, stopping"
                )
                return
            case .connectionError:
                // Despite multiple connection attempts we are still getting connection issues.
                // Connection error should be provided
                Logger.fileProviderExtension.info(
                    "\(user, privacy: .public) authentication try failed, no connection."
                )
                return
            case .success:
                Logger.fileProviderExtension.info(
                """
                Authenticated! Nextcloud account set up in File Provider extension.
                User: \(user, privacy: .public) at server: \(serverUrl, privacy: .public)
                """
                )
            }

            Task { @MainActor in
                ncAccount = account
                changeObserver = RemoteChangeObserver(
                    account: account,
                    remoteInterface: ncKit,
                    changeNotificationInterface: self,
                    domain: domain
                )
                ncKit.setup(delegate: changeObserver)
                signalEnumeratorAfterAccountSetup()
            }
        }
    }

    @objc func removeAccountConfig() {
        Logger.fileProviderExtension.info(
            "Received instruction to remove account data for user \(self.ncAccount!.username, privacy: .public) at server \(self.ncAccount!.serverUrl, privacy: .public)"
        )
        ncAccount = nil
    }

    func updatedSyncStateReporting(oldActions: Set<UUID>) {
        actionsLock.lock()

        guard oldActions.isEmpty != syncActions.isEmpty else {
            actionsLock.unlock()
            return
        }

        let command = "FILE_PROVIDER_DOMAIN_SYNC_STATE_CHANGE"
        var argument: String?
        if oldActions.isEmpty, !syncActions.isEmpty {
            argument = "SYNC_STARTED"
        } else if !oldActions.isEmpty, syncActions.isEmpty {
            argument = errorActions.isEmpty ? "SYNC_FINISHED" : "SYNC_FAILED"
            errorActions = []
        }
        
        actionsLock.unlock()

        guard let argument else { return }
        Logger.fileProviderExtension.debug("Reporting sync \(argument)")
        let message = command + ":" + argument + "\n"
        socketClient?.sendMessage(message)
    }
}

/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
        logger.debug("Serving supported service sources")
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
            logger.error("Could not get file provider manager for domain \(self.domain.displayName), cannot notify after account setup")
            return
        }

        assert(ncAccount != nil)

        fpManager.signalErrorResolved(NSFileProviderError(.notAuthenticated)) { error in
            if error != nil {
                self.logger.error("Error resolving not authenticated, received error: \(error!.localizedDescription)")
            }
        }

        logger.debug("Signalling enumerators for user \(self.ncAccount!.username) at server \(self.ncAccount!.serverUrl)")

        notifyChange()
    }

    func notifyChange() {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            logger.error("Could not get file provider manager for domain \(self.domain.displayName), cannot notify changes")
            return
        }

        fpManager.signalEnumerator(for: .workingSet) { error in
            if error != nil {
                self.logger.error("Error signalling enumerator for working set, received error: \(error!.localizedDescription)")
            }
        }
    }

    ///
    /// - Parameters:
    ///     - completionHandler: An optional completion handler which will be provided an error, if any occurred. Omitting this completion handler is fine, but you won't get notified of errors.
    ///
    @objc func setupDomainAccount(
        user: String,
        userId: String,
        serverUrl: String,
        password: String,
        userAgent: String = "Nextcloud-macOS/FileProviderExt",
        completionHandler: ((NSError?) -> Void)? = nil
    ) {
        let account = Account(user: user, id: userId, serverUrl: serverUrl, password: password)

        logger.info("Setting up domain account for user: \(user), userId: \(userId), serverUrl: \(serverUrl), password: \(password.isEmpty ? "<empty>" : "<not-empty>"), ncKitAccount: \(account.ncKitAccount)")

        guard account != ncAccount else {
            logger.info("Cancelling domain account setup because of receiving the same account information repeatedly!")
            completionHandler?(NSError(.invalidCredentials))
            return
        }

        guard password.isEmpty == false else {
            logger.info("Cancelling domain account setup because \"password\" is an empty string!")
            completionHandler?(NSError(.missingAccountInformation))
            return
        }

        guard serverUrl.isEmpty == false else {
            logger.info("Cancelling domain account setup because \"serverUrl\" is an empty string!")
            completionHandler?(NSError(.missingAccountInformation))
            return
        }

        guard user.isEmpty == false else {
            logger.info("Cancelling domain account setup because \"user\" is an empty string!")
            completionHandler?(NSError(.missingAccountInformation))
            return
        }

        guard userId.isEmpty == false else {
            logger.info("Cancelling domain account setup because \"userId\" is an empty string!")
            completionHandler?(NSError(.missingAccountInformation))
            return
        }

        // Store account information independently from the main app for later access.
        config.serverUrl = serverUrl
        config.user = user
        config.userId = userId
        keychain.savePassword(password, for: user, on: serverUrl)
        NextcloudKit.clearAccountErrorState(for: account.ncKitAccount)

        Task {
            ncKit.appendSession(
                account: account.ncKitAccount,
                urlBase: serverUrl,
                user: user,
                userId: userId,
                password: password,
                userAgent: userAgent,
                groupIdentifier: ""
            )

            var authAttemptState = AuthenticationAttemptResultState.connectionError // default

            // Retry a few times if we have a connection issue
            let options = NKRequestOptions(checkInterceptor: false)

            for authTimeout in AuthenticationTimeouts {
                authAttemptState = await ncKit.tryAuthenticationAttempt(account: account, options: options)

                guard authAttemptState == .connectionError else {
                    break
                }

                logger.info("\(user) authentication try timed out. Trying again soon.")
                try? await Task.sleep(nanoseconds: authTimeout)
            }

            switch (authAttemptState) {
                case .authenticationError:
                    logger.error("Authentication of \"\(user)\" failed due to bad credentials, cancelling domain account setup!")
                    completionHandler?(NSError(.invalidCredentials))
                    return
                case .connectionError:
                    // Despite multiple connection attempts we are still getting connection issues.
                    // Connection error should be provided
                    logger.error("Authentication of \"\(user)\" try failed, no connection.")
                    completionHandler?(NSError(.connection))
                    return
                case .success:
                    logger.info("Successfully authenticated! Nextcloud account set up in file provider extension. User: \(user) at server: \(serverUrl)")
            }

            Task { @MainActor in
                ncAccount = account
                dbManager = FilesDatabaseManager(account: account, fileProviderDomainIdentifier: domain.identifier, log: log)

                if let changeObserver {
                    changeObserver.invalidate()
                }

                if let dbManager {
                    changeObserver = RemoteChangeObserver(
                        account: account,
                        remoteInterface: ncKit,
                        changeNotificationInterface: self,
                        domain: domain,
                        dbManager: dbManager,
                        log: log
                    )
                } else {
                    logger.error("Invalid db manager, cannot start RCO")
                }

                ncKit.setup(groupIdentifier: Bundle.main.bundleIdentifier!, delegate: changeObserver)
                completionHandler?(nil)
                signalEnumeratorAfterAccountSetup()
            }
        }
    }

    @objc func removeAccountConfig() {
        logger.info("Received instruction to remove account data for user \(self.ncAccount!.username) at server \(self.ncAccount!.serverUrl)")
        ncAccount = nil
        dbManager = nil
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
        logger.debug("Reporting sync \(argument)")
        let message = command + ":" + argument + "\n"
        socketClient?.sendMessage(message)
    }
}

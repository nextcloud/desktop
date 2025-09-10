//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import AppKit
import FileProviderUI
import os

///
/// Our root view controller for the modal sheet Finder is presenting to the user for authentication.
///
class AuthenticationViewController: NSViewController {
    private var authenticationError: Error?

    @IBOutlet var activityDescription: NSTextField!
    @IBOutlet var cancellationButton: NSButton!
    @IBOutlet var progressIndicator: NSProgressIndicator!

    override func viewDidLoad() {
        super.viewDidLoad()

        activityDescription.stringValue = String(localized: "Authenticating…")
        cancellationButton.title = String(localized: "Cancel")
    }

    override func viewDidAppear() {
        super.viewDidAppear()
        progressIndicator.startAnimation(self) // This does not start automatically on macOS.
        dispatchAuthenticationTask()
    }

    ///
    /// Resolve the current extension context.
    ///
    override var extensionContext: FPUIActionExtensionContext {
        guard let parent = self.parent as? DocumentActionViewController else {
            fatalError("Parent view controller is not of expected type DocumentActionViewController!")
        }

        return parent.extensionContext
    }

    ///
    /// Action for the cancellation button in the user interface.
    ///
    @IBAction func cancel(_ sender: NSButton) {
        let code: FPUIExtensionErrorCode

        if let authenticationError = self.authenticationError {
            code = FPUIExtensionErrorCode.failed
        } else {
            code = FPUIExtensionErrorCode.userCancelled
        }

        let error = NSError(domain: FPUIErrorDomain, code: Int(code.rawValue))
        // extensionContext.cancelRequest(withError: error)
        extensionContext.completeRequest()
    }

    func dispatchAuthenticationTask() {
        Task {
            do {
                try await self.authenticate()
            } catch {
                updateViewsWithError(error)
            }
        }
    }

    func authenticate() async throws {
        guard let identifier = extensionContext.domainIdentifier else {
            fatalError("Extension context does not provide file provider domain identifier!")
        }

        let domain = NSFileProviderDomain(identifier: identifier, displayName: "")

        guard let manager = NSFileProviderManager(for: domain) else {
            fatalError("Failed to create file provider manager for domain with identifier \(identifier)!")
        }

        let url = try await manager.getUserVisibleURL(for: .rootContainer)

        let connection = try await serviceConnection(url: url, interruptionHandler: {
            Logger.authenticationViewController.error("Service connection interrupted")
        })

        if let error = await connection.authenticate() {
            Logger.authenticationViewController.error("An error was returned from the authentication call: \(error.localizedDescription, privacy: .public)")
            updateViewsWithError(error)
            return
        }

        Logger.authenticationViewController.info("Apparently, the authentication was successful.")
        extensionContext.completeRequest()
    }

    ///
    /// Update the activity description, hide the activity indicator and update the button title.
    ///
    func updateViewsWithError(_ error: Error) {
        authenticationError = error
        progressIndicator.isHidden = true
        activityDescription.stringValue = error.localizedDescription
        cancellationButton.title = String(localized: "Close")
    }
}

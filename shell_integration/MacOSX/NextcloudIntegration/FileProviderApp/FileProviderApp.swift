//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import os
import SwiftUI

@main
struct FileProviderApp: App {
    @State var accounts: [Account] = []

    let fileProviderDomainManager = FileProviderDomainManager()
    let logger = Logger(category: "FileProviderApp")

    init() {
        logger.debug("Initializing...")

        let arguments = ProcessInfo.processInfo.arguments
        var urls: [URL] = []

        for argument in arguments.dropFirst() { // Skip the first argument (it's the executable path)
            if let url = URL(string: argument), url.scheme == "http" || url.scheme == "https" { // Try to create a URL from the argument (HTTP/HTTPS web addresses)
                urls.append(url)
                logger.debug("Parsed URL: \(url.absoluteString)")
            } else {
                logger.error("Could not parse argument as HTTP(S) URL: \(argument)")
            }
        }

        for url in urls {
            guard var components = URLComponents(url: url, resolvingAgainstBaseURL: false) else {
                logger.error("Failed to parse URL components: \(url.absoluteString)")
                continue
            }

            guard let password = components.password else {
                logger.error("Failed to find a password in the URL: \(url.absoluteString)")
                continue
            }

            guard let user = components.user else {
                logger.error("Failed to find a user in the URL: \(url.absoluteString)")
                continue
            }

            components.password = nil
            components.user = nil

            guard let address = components.url else {
                logger.error("Failed to get address from components: \(url.absoluteString)")
                continue
            }

            accounts.append(Account(address: address, password: password, user: user))
        }

        Task { [self] in
            do {
                try await fileProviderDomainManager.removeAll()

                for account in accounts {
                    let domain = try await fileProviderDomainManager.add(for: account)
                    account.domain = domain
                }
            } catch {
                logger.error("Failed to update file provider domains: \(error)")
            }
        }
    }
    
    var body: some Scene {
        WindowGroup {
            ContentView(accounts: $accounts)
        }
    }
}


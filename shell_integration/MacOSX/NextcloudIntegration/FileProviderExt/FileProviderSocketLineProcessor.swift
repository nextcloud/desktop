/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
import NCDesktopClientSocketKit
import OSLog

class FileProviderSocketLineProcessor: NSObject, LineProcessor {
    var delegate: FileProviderExtension

    required init(delegate: FileProviderExtension) {
        self.delegate = delegate
    }

    func process(_ line: String) {
        if line.contains("~") {  // We use this as the separator specifically in ACCOUNT_DETAILS
            Logger.desktopClientConnection.debug(
                "Processing file provider line with potentially sensitive user data")
        } else {
            Logger.desktopClientConnection.debug(
                "Processing file provider line: \(line, privacy: .public)")
        }

        let splitLine = line.split(separator: ":", maxSplits: 1)
        guard let commandSubsequence = splitLine.first else {
            Logger.desktopClientConnection.error("Input line did not have a first element")
            return
        }
        let command = String(commandSubsequence)

        Logger.desktopClientConnection.debug("Received command: \(command, privacy: .public)")
        if command == "SEND_FILE_PROVIDER_DOMAIN_IDENTIFIER" {
            delegate.sendFileProviderDomainIdentifier()
        } else if command == "ACCOUNT_NOT_AUTHENTICATED" {
            delegate.removeAccountConfig()
        } else if command == "ACCOUNT_DETAILS" {
            guard let accountDetailsSubsequence = splitLine.last else { return }
            let splitAccountDetails = accountDetailsSubsequence.split(separator: "~", maxSplits: 2)

            let user = String(splitAccountDetails[0])
            let serverUrl = String(splitAccountDetails[1])
            let password = String(splitAccountDetails[2])

            delegate.setupDomainAccount(user: user, serverUrl: serverUrl, password: password)
        }
    }
}

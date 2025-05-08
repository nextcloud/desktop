/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import Foundation
import NCDesktopClientSocketKit
import NextcloudFileProviderKit
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
            guard let accountDetailsSubsequence = splitLine.last else {
                Logger.desktopClientConnection.error("Account details did not have a first element")
                return
            }
            let splitAccountDetails = accountDetailsSubsequence.split(separator: "~", maxSplits: 4)

            let userAgent = String(splitAccountDetails[0])
            let user = String(splitAccountDetails[1])
            let userId = String(splitAccountDetails[2])
            let serverUrl = String(splitAccountDetails[3])
            let password = String(splitAccountDetails[4])

            delegate.setupDomainAccount(
                user: user,
                userId: userId,
                serverUrl: serverUrl,
                password: password,
                userAgent: userAgent
            )
        } else if command == "IGNORE_LIST" {
            guard let ignoreListSubsequence = splitLine.last else {
                Logger.desktopClientConnection.error("Ignore list missing contents!")
                return
            }
            let ignoreList = ignoreListSubsequence.split(separator: "\n").map { String($0) }
            delegate.ignoredFiles = IgnoredFilesMatcher(ignoreList: ignoreList)
        }
    }
}

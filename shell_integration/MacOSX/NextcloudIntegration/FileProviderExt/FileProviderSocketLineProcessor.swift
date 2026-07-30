//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NCDesktopClientSocketKit
import NextcloudFileProviderKit
import OSLog

class FileProviderSocketLineProcessor: NSObject, LineProcessor {
    var delegate: FileProviderExtension
    let log: any FileProviderLogging
    let logger: FileProviderLogger

    required init(delegate: FileProviderExtension, log: any FileProviderLogging) {
        self.delegate = delegate
        self.log = log
        self.logger = FileProviderLogger(category: "FileProviderSocketLineProcessor", log: log)
    }

    func process(_ line: String) {
        if line.contains("~") {  // We use this as the separator specifically in ACCOUNT_DETAILS
            logger.debug("Processing file provider line with potentially sensitive user data")
        } else {
            logger.debug("Processing file provider line: \(line)")
        }

        let splitLine = line.split(separator: ":", maxSplits: 1)
        guard let commandSubsequence = splitLine.first else {
            logger.error("Input line did not have a first element")
            return
        }
        let command = String(commandSubsequence)

        logger.debug("Received command: \(command)")
        if command == "SEND_FILE_PROVIDER_DOMAIN_IDENTIFIER" {
            delegate.sendFileProviderDomainIdentifier()
        } else if command == "ACCOUNT_NOT_AUTHENTICATED" {
            delegate.removeAccountConfig()
        } else if command == "ACCOUNT_DETAILS" {
            guard let accountDetailsSubsequence = splitLine.last else {
                logger.error("Account details did not have a first element")
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
                logger.error("Ignore list missing contents!")
                return
            }
            let ignoreList = ignoreListSubsequence.components(separatedBy: "_~IL$~_")
            logger.debug("Applying \(ignoreList.count) ignore file patterns.")
            delegate.ignoredFiles = IgnoredFilesMatcher(ignoreList: ignoreList, log: log)
        }
    }
}

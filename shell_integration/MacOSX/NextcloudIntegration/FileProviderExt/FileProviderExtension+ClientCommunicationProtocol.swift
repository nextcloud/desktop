//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudFileProviderKit

extension FileProviderExtension: ClientCommunicationProtocol {
    func hasDirtyUserData(completionHandler: ((Bool) -> Void)?) {
        logger.debug("Dirty user data check is requested.")

        guard let completionHandler else {
            logger.error("Cannot check for dirty user data, completion handler is nil.")
            return
        }

        guard let manager = NSFileProviderManager(for: domain) else {
            logger.error("Cannot check for dirty user data, file provider manager unavailable.")
            completionHandler(false)
            return
        }

        let observer = DirtyUserDataObserver(log: log, completionHandler: completionHandler)
        let page = NSFileProviderPage(NSFileProviderPage.initialPageSortedByName as Data)
        let enumerator = manager.enumeratorForMaterializedItems()
        enumerator.enumerateItems(for: observer, startingAt: page)
    }

    func getFileProviderDomainIdentifier(completionHandler: @escaping (String?, Error?) -> Void) {
        logger.debug("Returning file provider domain identifier.", [.domain: domain.identifier.rawValue])
        completionHandler(domain.identifier.rawValue, nil)
    }

    func configureAccount(withUser user: String, userId: String, serverUrl: String, password: String, userAgent: String) {
        logger.info("Received account to configure.")
        setupDomainAccount(user: user, userId: userId, serverUrl: serverUrl, password: password, userAgent: userAgent)
    }

    func removeAccountConfig() {
        logger.info("Received request to remove account data.")
        dbManager = nil
        ncAccount = nil
    }

    func setIgnoreList(_ ignoreList: [String]) {
        ignoredFiles = IgnoredFilesMatcher(ignoreList: ignoreList, log: log)
        logger.info("Ignore list updated.")
    }
}

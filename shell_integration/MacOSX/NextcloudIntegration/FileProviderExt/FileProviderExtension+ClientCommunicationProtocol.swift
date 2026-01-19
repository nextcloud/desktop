//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import NextcloudFileProviderKit

extension FileProviderExtension: ClientCommunicationProtocol {
    func getFileProviderDomainIdentifier(completionHandler: @escaping (String?, Error?) -> Void) {
        logger.debug("Returning file provider domain identifier.", [.domain: domain.identifier.rawValue])
        completionHandler(domain.identifier.rawValue, nil)
    }

    func configureAccount(withUser user: String, userId: String, serverUrl: String, password: String, userAgent: String) {
        logger.info("Received account to configure.")
        setupDomainAccount(user: user, userId: userId, serverUrl: serverUrl, password: password, userAgent: userAgent)
    }

    func removeAccountConfig() {
        logger.info("Received request to remove account data for user \(ncAccount!.username) at server \(ncAccount!.serverUrl)")
        ncAccount = nil
        dbManager = nil
    }

    func getTrashDeletionEnabledState(completionHandler: @escaping (Bool, Bool) -> Void) {
        let enabled = config.trashDeletionEnabled
        let set = config.trashDeletionSet
        completionHandler(enabled, set)
    }

    func setTrashDeletionEnabled(_ enabled: Bool) {
        config.trashDeletionEnabled = enabled
        logger.info("Trash deletion setting changed to: \(enabled)")
    }

    func setIgnoreList(_ ignoreList: [String]) {
        ignoredFiles = IgnoredFilesMatcher(ignoreList: ignoreList, log: log)
        logger.info("Ignore list updated.")
    }
}

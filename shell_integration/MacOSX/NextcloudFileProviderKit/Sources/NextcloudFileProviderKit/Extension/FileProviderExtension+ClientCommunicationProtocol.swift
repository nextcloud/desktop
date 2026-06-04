//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudFileProviderXPC

extension FileProviderExtension: ClientCommunicationProtocol {
    public func hasDirtyUserData(completionHandler: ((Bool) -> Void)?) {
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

    public func getFileProviderDomainIdentifier(completionHandler: @escaping (String?, Error?) -> Void) {
        logger.debug("Returning file provider domain identifier.", [.domain: domain.identifier.rawValue])
        completionHandler(domain.identifier.rawValue, nil)
    }

    public func configureAccount(withUser user: String, userId: String, serverUrl: String, password: String, userAgent: String) {
        logger.info("Received account to configure.")
        setupDomainAccount(user: user, userId: userId, serverUrl: serverUrl, password: password, userAgent: userAgent)
    }

    public func removeAccountConfig() {
        logger.info("Received request to remove account data.")
        dbManager = nil
        ncAccount = nil
    }

    public func setIgnoreList(_ ignoreList: [String]) {
        ignoredFiles = IgnoredFilesMatcher(ignoreList: ignoreList, log: log)
        logger.info("Ignore list updated.")
    }

    public func processFileIdsChanged(_ fileIds: [NSNumber], completionHandler: ((Bool) -> Void)?) {
        guard let completionHandler else {
            logger.error("Cannot process file ID changes, completion handler is nil.")
            return
        }

        guard fileIds.isEmpty == false else {
            logger.info("Received file change notification without file IDs. Signalling enumerator.")
            notifyChange()
            completionHandler(true)
            return
        }

        guard let dbManager else {
            logger.info("Received file ID changes before database setup. Signalling enumerator.")
            notifyChange()
            completionHandler(true)
            return
        }

        let changedFileIds = Set(fileIds.map(\.stringValue))

        guard dbManager.containsAnyItemMetadata(fileIds: changedFileIds) else {
            logger.debug("Ignoring file ID changes because no locally known metadata matched.")
            completionHandler(true)
            return
        }

        logger.info("Received file ID changes for locally known metadata. Signalling enumerator.")
        notifyChange()
        completionHandler(true)
    }
}

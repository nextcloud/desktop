//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudFileProviderKit
import OSLog

extension FileProviderExtension: NSFileProviderCustomAction {
    func performAction(
        identifier actionIdentifier: NSFileProviderExtensionActionIdentifier,
        onItemsWithIdentifiers itemIdentifiers: [NSFileProviderItemIdentifier],
        completionHandler: @escaping ((any Error)?) -> Void
    ) -> Progress {
        switch actionIdentifier.rawValue {
        case "com.nextcloud.desktopclient.FileProviderExt.KeepDownloadedAction":
            return performKeepDownloadedAction(
                keepDownloaded: true,
                onItemsWithIdentifiers: itemIdentifiers,
                completionHandler: completionHandler
            )
        case "com.nextcloud.desktopclient.FileProviderExt.AutoEvictAction":
            return performKeepDownloadedAction(
                keepDownloaded: false,
                onItemsWithIdentifiers: itemIdentifiers,
                completionHandler: completionHandler
            )
        default:
            logger.error("Unsupported action: \(actionIdentifier.rawValue)")
            completionHandler(NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError))
            return Progress()
        }
    }
    
    private func performKeepDownloadedAction(
        keepDownloaded: Bool,
        onItemsWithIdentifiers itemIdentifiers: [NSFileProviderItemIdentifier],
        completionHandler: @escaping ((any Error)?) -> Void
    ) -> Progress {
        guard let ncAccount else {
            logger.error("Not setting keep offline for items, account not set up yet.")
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }
        guard let dbManager else {
            logger.error("Not setting keep offline for items as database is unreachable.")
            completionHandler(NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        let progress = Progress()

        // If there are no items, complete successfully immediately.
        if itemIdentifiers.isEmpty {
            logger.info("No items to process for keepDownloaded action.")
            completionHandler(nil)
            return progress
        }

        // Explicitly set totalUnitCount for clarity, though addChild with pendingUnitCount also defines this.
        progress.totalUnitCount = Int64(itemIdentifiers.count)

        Task {
            let localNcKit = self.ncKit
            let localDomain = self.domain

            do {
                try await withThrowingTaskGroup(of: Void.self) { group in
                    for identifier in itemIdentifiers {
                        group.addTask {
                            // This task processes one item.
                            guard let item = await Item.storedItem(
                                identifier: identifier,
                                account: ncAccount,
                                remoteInterface: localNcKit,
                                dbManager: dbManager,
                                log: self.log
                            ) else {
                                throw NSError.fileProviderErrorForNonExistentItem(
                                    withIdentifier: identifier
                                )
                            }
                            try await item.set(keepDownloaded: keepDownloaded, domain: localDomain)
                        }
                    }

                    for try await result in group {
                        progress.completedUnitCount = 1
                    }
                }
                logger.info("All items successfully processed for keepDownloaded=\(keepDownloaded)")
                completionHandler(nil)
            } catch let error {
                logger.error("Error during keepDownloaded=\(keepDownloaded) action: \(error.localizedDescription)")
                completionHandler(error)
            }
        }
        return progress
    }
}

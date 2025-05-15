/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

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
            Logger.fileProviderExtension.error("Unsupported action: \(actionIdentifier.rawValue)")
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
            Logger.fileProviderExtension.error(
                "Not setting keep offline for items, account not set up yet."
            )
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }
        guard let dbManager else {
            Logger.fileProviderExtension.error(
                "Not setting keep offline for items as database is unreachable."
            )
            completionHandler(NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        let progress = Progress()
        for identifier in itemIdentifiers {
            guard let item = Item.storedItem(
                identifier: identifier,
                account: ncAccount,
                remoteInterface: ncKit,
                dbManager: dbManager
            ) else {
                let error = NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier)
                completionHandler(error)
                return progress
            }

            let childProgress = Progress()
            progress.addChild(childProgress, withPendingUnitCount: 1)
            Task {
                do {
                    try await item.set(keepDownloaded: keepDownloaded, domain: domain)
                    childProgress.completedUnitCount = 1
                } catch let error {
                    completionHandler(error)
                }
            }
        }
        return progress
    }
}

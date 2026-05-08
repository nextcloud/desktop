//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import OSLog

extension FileProviderExtension: NSFileProviderCustomAction {
    public func performAction(
        identifier actionIdentifier: NSFileProviderExtensionActionIdentifier,
        onItemsWithIdentifiers itemIdentifiers: [NSFileProviderItemIdentifier],
        completionHandler: @Sendable @escaping ((any Error)?) -> Void
    ) -> Progress {
        switch actionIdentifier.rawValue {
            case "com.nextcloud.desktopclient.FileProviderExt.FileActionsAction":
                guard let itemIdentifier = itemIdentifiers.first else {
                    logger.error("Failed to get first item identifier for file actions action.")
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                guard let dbManager else {
                    logger.error("Cannot fetch metadata for item file actions due to database manager not being available.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.cannotSynchronize))
                    return Progress()
                }

                Task {
                    guard let userVisibleURL = try await manager?.getUserVisibleURL(for: itemIdentifier) else {
                        logger.error("Failed to get user-visible URL for item.", [.item: itemIdentifier])
                        completionHandler(NSFileProviderError(.noSuchItem))
                        return
                    }

                    guard let metadata = dbManager.itemMetadata(itemIdentifier) else {
                        logger.error("Failed to get metadata for item.", [.item: itemIdentifier])
                        completionHandler(NSFileProviderError(.cannotSynchronize))
                        return
                    }

                    let path = userVisibleURL.path
                    let domainIdentifier = domain.identifier.rawValue
                    logger.info("Telling main app to present file actions.", [.item: path, .domain: domainIdentifier])
                    app?.presentFileActions(metadata.ocId, path: path, remoteItemPath: metadata.path, withDomainIdentifier: domainIdentifier)
                    completionHandler(nil)
                }

                return Progress()
            case "com.nextcloud.desktopclient.FileProviderExt.KeepDownloadedAction":
                return performKeepDownloadedAction(keepDownloaded: true, onItemsWithIdentifiers: itemIdentifiers, completionHandler: completionHandler)
            case "com.nextcloud.desktopclient.FileProviderExt.AutoEvictAction":
                return performKeepDownloadedAction(keepDownloaded: false, onItemsWithIdentifiers: itemIdentifiers, completionHandler: completionHandler)
            case "com.nextcloud.desktopclient.FileProviderExt.EvictAction":
                return performEvictAction(onItemsWithIdentifiers: itemIdentifiers, completionHandler: completionHandler)
            default:
                logger.error("Unsupported action: \(actionIdentifier.rawValue)")
                completionHandler(NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError))
                return Progress()
        }
    }

    private func performKeepDownloadedAction(
        keepDownloaded: Bool,
        onItemsWithIdentifiers itemIdentifiers: [NSFileProviderItemIdentifier],
        completionHandler: @Sendable @escaping ((any Error)?) -> Void
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

                    for try await _ in group {
                        progress.completedUnitCount += 1
                    }
                }
                logger.info("All items successfully processed for keepDownloaded=\(keepDownloaded)")
                completionHandler(nil)
            } catch {
                logger.error("Error during keepDownloaded=\(keepDownloaded) action: \(error.localizedDescription)")
                completionHandler(error)
            }
        }
        return progress
    }

    ///
    /// Force the materialized payload of an item to be removed (made dataless), even when the item is pinned via "Always keep downloaded" (#9891).
    ///
    /// macOS refuses `evictItem` on items whose `contentPolicy` is`.downloadEagerlyAndKeepDownloaded`.
    /// To honour the user's explicit "Remove download" gesture, we first clear the keep-downloaded flag — which flips `contentPolicy` back to `.inherited` and signals the framework — and then evict.
    /// The unpin is propagated to descendants by `Item.set(keepDownloaded:domain:)` (matches `AutoEvictAction` semantics), so directories behave consistently with the pin counterpart.
    ///
    private func performEvictAction(onItemsWithIdentifiers itemIdentifiers: [NSFileProviderItemIdentifier], completionHandler: @Sendable @escaping ((any Error)?) -> Void) -> Progress {
        guard let ncAccount else {
            logger.error("Not removing downloads because account is not set up yet.")
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let dbManager else {
            logger.error("Not removing downloads because database is unreachable.")
            completionHandler(NSFileProviderError(.cannotSynchronize))
            return Progress()
        }

        guard let manager else {
            logger.error("Not removing downloads because file provider manager is not available.")
            completionHandler(NSFileProviderError(.providerNotFound))
            return Progress()
        }

        let progress = Progress()

        if itemIdentifiers.isEmpty {
            logger.info("No items to process for remove download action.")
            completionHandler(nil)
            return progress
        }

        progress.totalUnitCount = Int64(itemIdentifiers.count)

        Task {
            let localNcKit = self.ncKit
            let localDomain = self.domain

            do {
                try await withThrowingTaskGroup(of: Void.self) { group in
                    for identifier in itemIdentifiers {
                        group.addTask {
                            guard let item = await Item.storedItem(identifier: identifier, account: ncAccount, remoteInterface: localNcKit, dbManager: dbManager, log: self.log) else {
                                throw NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier)
                            }

                            // Clear keep-downloaded so that contentPolicy changes from `.downloadEagerlyAndKeepDownloaded` back to `.inherited`. `set(keepDownloaded:)` awaits the framework's acknowledgement of the modification, so by the time it returns the policy refresh has propagated.
                            if item.keepDownloaded {
                                try await item.set(keepDownloaded: false, domain: localDomain)
                            }

                            try await manager.evictItem(identifier: identifier)
                        }
                    }

                    for try await _ in group {
                        progress.completedUnitCount += 1
                    }
                }

                logger.info("All items successfully processed by evict action.")
                completionHandler(nil)
            } catch {
                logger.error("Error during eviction: \(error.localizedDescription)")
                completionHandler(error)
            }
        }

        return progress
    }
}

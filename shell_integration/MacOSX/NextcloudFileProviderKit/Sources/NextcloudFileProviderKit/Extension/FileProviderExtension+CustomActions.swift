//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import OSLog

extension FileProviderExtension: NSFileProviderCustomAction {
    public func performAction(identifier actionIdentifier: NSFileProviderExtensionActionIdentifier, onItemsWithIdentifiers itemIdentifiers: [NSFileProviderItemIdentifier], completionHandler: @Sendable @escaping ((any Error)?) -> Void) -> Progress {
        switch actionIdentifier {
            case .fileActions:
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
            case .openInBrowser:
                guard let itemIdentifier = itemIdentifiers.first else {
                    logger.error("Failed to get first item identifier for open in browser action.")
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                guard let dbManager else {
                    logger.error("Cannot fetch metadata for open in browser action due to database manager not being available.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.cannotSynchronize))
                    return Progress()
                }

                guard let metadata = dbManager.itemMetadata(itemIdentifier) else {
                    logger.error("Failed to get metadata for open in browser action.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                // The numeric file id is required to build the deprecated
                // fallback URL the main app uses when PROPFIND for the
                // server-side `privatelink` property is unavailable. Without
                // it, the main app would have nothing to open.
                guard !metadata.fileId.isEmpty else {
                    logger.error("Cannot open item in browser because the metadata has no fileId.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                let domainIdentifier = domain.identifier.rawValue
                logger.info("Telling main app to open item in browser.", [.item: metadata.path, .domain: domainIdentifier])
                app?.openItemInBrowser(metadata.fileId, remoteItemPath: metadata.path, forDomainIdentifier: domainIdentifier)
                completionHandler(nil)

                return Progress()
            case .copyInternalLink:
                guard let itemIdentifier = itemIdentifiers.first else {
                    logger.error("Failed to get first item identifier for copy internal link action.")
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                guard let dbManager else {
                    logger.error("Cannot fetch metadata for copy internal link action due to database manager not being available.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.cannotSynchronize))
                    return Progress()
                }

                guard let metadata = dbManager.itemMetadata(itemIdentifier) else {
                    logger.error("Failed to get metadata for copy internal link action.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                // The numeric file id is required to build the deprecated
                // fallback URL the main app uses when PROPFIND for the
                // server-side `privatelink` property is unavailable. Without
                // it, the main app would have nothing to copy. Matches the
                // equivalent guard in `case .openInBrowser`.
                guard !metadata.fileId.isEmpty else {
                    logger.error("Cannot copy internal link because the metadata has no fileId.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                let domainIdentifier = domain.identifier.rawValue
                logger.info("Telling main app to copy internal link.", [.item: metadata.path, .domain: domainIdentifier])
                app?.copyInternalLink(forItem: metadata.fileId, remoteItemPath: metadata.path, forDomainIdentifier: domainIdentifier)
                completionHandler(nil)

                return Progress()
            case .keepDownloaded:
                return performKeepDownloadedAction(keepDownloaded: true, onItemsWithIdentifiers: itemIdentifiers, completionHandler: completionHandler)
            case .evictAutomatically:
                return performKeepDownloadedAction(keepDownloaded: false, onItemsWithIdentifiers: itemIdentifiers, completionHandler: completionHandler)
            case .evict, .evictDescendants:
                // Same handler for both — the folder variant exists only to carry
                // a folder-appropriate menu label ("Remove downloaded items").
                // `evictItem` recurses into a directory's contents (#10085).
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
    /// Handle "Remove download" (a file) and "Remove downloaded items" (a folder or the root) — both routed here (#9891, #10085).
    ///
    /// macOS refuses `evictItem` on items whose `contentPolicy` is `.downloadEagerlyAndKeepDownloaded`.
    ///
    /// - For a **file**: to honour the user's explicit gesture even on a pinned file, we first clear its keep-downloaded flag — which flips `contentPolicy` back to `.inherited` and signals the framework — and then evict it.
    /// - For a **directory** (including the root): we evict each *evictable* (downloaded, non-pinned) descendant file individually rather than calling `evictItem` on the directory. This leaves individually-pinned descendants materialized (honouring the explicit per-item choice), never asks the framework to evict strict-pinned content (so it cannot hit -2008), and does not rely on `evictItem` recursing into a directory or the root pseudo-container.
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

                            if item.metadata.directory {
                                // "Remove downloaded items" on a folder or the root: evict each
                                // evictable (downloaded, non-pinned) descendant file individually.
                                // This leaves individually-pinned descendants materialized —
                                // honoring the user's explicit "Always keep downloaded" — and never
                                // asks the framework to evict strict-pinned content, so it cannot hit
                                // -2008 NonEvictable (#9891). It also does not depend on `evictItem`
                                // recursing into a directory (incl. the root pseudo-container) (#10085).
                                for descendant in dbManager.evictableDescendantFileIdentifiers(directoryMetadata: item.metadata) {
                                    try await manager.evictItem(identifier: descendant)
                                }
                            } else {
                                // "Remove download" on a single file. Clear keep-downloaded first so
                                // contentPolicy changes from `.downloadEagerlyAndKeepDownloaded` back
                                // to `.inherited`; `set(keepDownloaded:)` awaits the framework's
                                // acknowledgement, so the policy refresh has propagated by the time it
                                // returns (#9891).
                                if item.keepDownloaded {
                                    try await item.set(keepDownloaded: false, domain: localDomain)
                                }

                                try await manager.evictItem(identifier: identifier)
                            }
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

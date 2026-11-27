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

                Task {
                    // Account setup only needs to wait for the metadata lookup below.
                    let userVisibleURL: URL?
                    do {
                        userVisibleURL = try await manager?.getUserVisibleURL(for: itemIdentifier)
                    } catch {
                        completionHandler(error)
                        return
                    }
                    guard let userVisibleURL else {
                        logger.error("Failed to get user-visible URL for item.", [.item: itemIdentifier])
                        completionHandler(NSFileProviderError(.noSuchItem))
                        return
                    }

                    guard let (dbManager, operation) = beginCustomAction(completionHandler) else { return }
                    defer { operation.finish() }

                    guard let metadata = dbManager.itemMetadata(itemIdentifier) else {
                        logger.error("Failed to get metadata for item.", [.item: itemIdentifier])
                        completionHandler(NSFileProviderError(.cannotSynchronize))
                        return
                    }

                    let path = userVisibleURL.path
                    let domainIdentifier = domain.identifier.rawValue
                    logger.info("Telling main app to present file actions.", [.item: path, .domain: domainIdentifier])
                    app?.presentFileActions(
                        metadata.ocId,
                        path: path,
                        remoteItemPath: metadata.davParentPathWithoutOrigin,
                        withDomainIdentifier: domainIdentifier
                    )
                    completionHandler(nil)
                }

                return Progress()
            case .openInBrowser:
                guard let itemIdentifier = itemIdentifiers.first else {
                    logger.error("Failed to get first item identifier for open in browser action.")
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                guard let (dbManager, operation) = beginCustomAction(completionHandler) else { return Progress() }
                defer { operation.finish() }

                guard let metadata = dbManager.itemMetadata(itemIdentifier) else {
                    logger.error("Failed to get metadata for open in browser action.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                // The main app needs the numeric file ID to build the internal link.
                guard !metadata.fileId.isEmpty else {
                    logger.error("Cannot open item in browser because the metadata has no fileId.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                let domainIdentifier = domain.identifier.rawValue
                let parentPathWithoutOrigin = metadata.davParentPathWithoutOrigin
                logger.info("Telling main app to open item in browser.", [.item: parentPathWithoutOrigin, .domain: domainIdentifier])
                app?.openItemInBrowser(metadata.fileId, remoteItemPath: parentPathWithoutOrigin, forDomainIdentifier: domainIdentifier)
                completionHandler(nil)

                return Progress()
            case .copyInternalLink:
                guard let itemIdentifier = itemIdentifiers.first else {
                    logger.error("Failed to get first item identifier for copy internal link action.")
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                guard let (dbManager, operation) = beginCustomAction(completionHandler) else { return Progress() }
                defer { operation.finish() }

                guard let metadata = dbManager.itemMetadata(itemIdentifier) else {
                    logger.error("Failed to get metadata for copy internal link action.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                // The main app needs the numeric file ID to build the internal link.
                guard !metadata.fileId.isEmpty else {
                    logger.error("Cannot copy internal link because the metadata has no fileId.", [.item: itemIdentifier])
                    completionHandler(NSFileProviderError(.noSuchItem))
                    return Progress()
                }

                let domainIdentifier = domain.identifier.rawValue
                let parentPathWithoutOrigin = metadata.davParentPathWithoutOrigin
                logger.info("Telling main app to copy internal link.", [.item: parentPathWithoutOrigin, .domain: domainIdentifier])
                app?.copyInternalLink(forItem: metadata.fileId, remoteItemPath: parentPathWithoutOrigin, forDomainIdentifier: domainIdentifier)
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

    private func beginCustomAction(_ completionHandler: (Error?) -> Void) -> (FilesDatabaseManager, SyncOperations.Operation)? {
        do {
            return try beginSyncOperation()
        } catch {
            completionHandler(error)
            return nil
        }
    }

    private func performKeepDownloadedAction(
        keepDownloaded: Bool,
        onItemsWithIdentifiers itemIdentifiers: [NSFileProviderItemIdentifier],
        completionHandler: @Sendable @escaping ((any Error)?) -> Void
    ) -> Progress {
        guard let (dbManager, operation) = beginCustomAction(completionHandler) else { return Progress() }

        let progress = Progress()

        // If there are no items, complete successfully immediately.
        if itemIdentifiers.isEmpty {
            logger.info("No items to process for keepDownloaded action.")
            completionHandler(nil)
            operation.finish()
            return progress
        }

        // Explicitly set totalUnitCount for clarity, though addChild with pendingUnitCount also defines this.
        progress.totalUnitCount = Int64(itemIdentifiers.count)

        operation.run { [self] in
            let localNcKit = self.ncKit
            let localDomain = self.domain

            do {
                try await withThrowingTaskGroup(of: Void.self) { group in
                    for identifier in itemIdentifiers {
                        group.addTask {
                            let item = try await Item.lookupItemAndFindParentIdentifier(
                                identifier: identifier,
                                account: dbManager.account,
                                remoteInterface: localNcKit,
                                dbManager: dbManager,
                                allowsRemoteLookup: !self.blockSync,
                                taskHandler: { operation.registerReadTask($0) },
                                log: self.log
                            )
                            guard !operation.isStopped else { throw NSFileProviderError(.serverUnreachable) }
                            try await item.set(keepDownloaded: keepDownloaded, domain: localDomain)
                        }
                    }

                    for try await _ in group {
                        progress.completedUnitCount += 1
                    }
                }
                guard !retryIfStopped(operation, completion: completionHandler) else { return }
                logger.info("All items successfully processed for keepDownloaded=\(keepDownloaded)")
                completionHandler(nil)
            } catch {
                guard !retryIfStopped(operation, completion: completionHandler) else { return }
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

        guard let (dbManager, operation) = beginCustomAction(completionHandler) else { return progress }
        operation.run { [self] in
            let localNcKit = self.ncKit
            let localDomain = self.domain

            do {
                try await withThrowingTaskGroup(of: Void.self) { group in
                    for identifier in itemIdentifiers {
                        group.addTask {
                            let itemsToEvict: [NSFileProviderItemIdentifier]
                            let item = try await Item.lookupItemAndFindParentIdentifier(
                                identifier: identifier,
                                account: dbManager.account,
                                remoteInterface: localNcKit,
                                dbManager: dbManager,
                                allowsRemoteLookup: !self.blockSync,
                                taskHandler: { operation.registerReadTask($0) },
                                log: self.log
                            )
                            guard !operation.isStopped else { throw NSFileProviderError(.serverUnreachable) }
                            if item.metadata.directory {
                                // Leave individually pinned files downloaded.
                                itemsToEvict = dbManager.evictableDescendantFileIdentifiers(directoryMetadata: item.metadata)
                            } else {
                                if item.keepDownloaded {
                                    try await item.set(keepDownloaded: false, domain: localDomain)
                                }
                                itemsToEvict = [identifier]
                            }

                            for itemIdentifier in itemsToEvict {
                                guard !operation.isStopped else { throw NSFileProviderError(.serverUnreachable) }
                                try await manager.evictItem(identifier: itemIdentifier)
                            }
                        }
                    }

                    for try await _ in group {
                        progress.completedUnitCount += 1
                    }
                }

                guard !retryIfStopped(operation, completion: completionHandler) else { return }
                logger.info("All items successfully processed by evict action.")
                completionHandler(nil)
            } catch {
                guard !retryIfStopped(operation, completion: completionHandler) else { return }
                logger.error("Error during eviction: \(error.localizedDescription)")
                completionHandler(error)
            }
        }

        return progress
    }
}

//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider

public extension Item {
    func toggle(keepDownloadedIn domain: NSFileProviderDomain) async throws {
        try await set(keepDownloaded: !keepDownloaded, domain: domain)
    }

    ///
    /// Apply or clear the "keep downloaded" state for this item and, when it is
    /// a directory, recursively for every descendant.
    ///
    /// When enabling on a directory, a recursive PROPFIND is performed first so
    /// the local database learns about every descendant on the server — even
    /// those that were never enumerated locally. This makes the user's intent
    /// ("pin the whole subtree") match reality regardless of prior browsing.
    ///
    /// When disabling, no server round-trip is needed: the flag is cleared on
    /// every descendant the database already knows about.
    ///
    /// For every file descendant (including the item itself if it is a file),
    /// `requestDownloadForItem` is issued when the payload is not yet local, so
    /// content is pulled down immediately and automatically rather than lazily.
    ///
    func set(keepDownloaded: Bool, domain: NSFileProviderDomain) async throws {
        _ = try dbManager.set(keepDownloaded: keepDownloaded, for: metadata)

        guard let manager = NSFileProviderManager(for: domain) else {
            if #available(iOS 17.1, macOS 14.1, *) {
                throw NSFileProviderError(.providerDomainNotFound)
            } else {
                let providerDomainNotFoundErrorCode = -2013
                throw NSError(
                    domain: NSFileProviderErrorDomain,
                    code: providerDomainNotFoundErrorCode,
                    userInfo: [NSLocalizedDescriptionKey: "Failed to get manager for domain."]
                )
            }
        }

        // When enabling on a directory, discover every descendant on the server
        // so we can flip the flag and request downloads for items that were
        // never enumerated locally yet. Skip on disable — turning a flag off
        // does not justify a potentially expensive recursive PROPFIND.
        //
        // Walk the tree folder-by-folder with depth-1 PROPFINDs rather than a
        // single infinite-depth call: infinite-depth is explicitly rejected by
        // `Enumerator.readServerUrl` (see `readServerUrl`'s trailing branch)
        // because it can produce extreme server load on large subtrees.
        if metadata.directory, keepDownloaded {
            try await enumerateSubtreeBreadthFirst(domain: domain)
        }

        try await signalKeepDownloaded(
            keepDownloaded: keepDownloaded,
            identifier: itemIdentifier,
            isDirectory: metadata.directory,
            isDownloaded: isDownloaded,
            manager: manager
        )

        guard metadata.directory else { return }

        // `childItems` matches on `serverUrl.starts(with:)`, so a single call
        // returns every descendant at any depth — no manual recursion needed.
        for child in dbManager.childItems(directoryMetadata: metadata) {
            do {
                _ = try dbManager.set(keepDownloaded: keepDownloaded, for: child)
            } catch {
                // A single failed descendant must not abort the subtree.
                logger.error(
                    "Could not update keep-downloaded flag on descendant.",
                    [.item: child.ocId, .name: child.fileName, .error: error]
                )
                continue
            }

            do {
                try await signalKeepDownloaded(
                    keepDownloaded: keepDownloaded,
                    identifier: NSFileProviderItemIdentifier(child.ocId),
                    isDirectory: child.directory,
                    isDownloaded: child.downloaded,
                    manager: manager
                )
            } catch {
                logger.error(
                    "Could not signal keep-downloaded change to framework for descendant.",
                    [.item: child.ocId, .name: child.fileName, .error: error]
                )
            }
        }
    }

    ///
    /// Ask the File Provider framework to realise a keep-downloaded change.
    ///
    /// When enabling and the item is a file whose payload is not yet local,
    /// request an immediate download; the framework will call back into our
    /// `fetchContents` to actually pull the bytes. Otherwise — for folders,
    /// already-downloaded files, and the disable path — just bump
    /// `lastUsedDate` so the framework re-evaluates eviction/pin state.
    ///
    private func signalKeepDownloaded(
        keepDownloaded: Bool,
        identifier: NSFileProviderItemIdentifier,
        isDirectory: Bool,
        isDownloaded: Bool,
        manager: NSFileProviderManager
    ) async throws {
        if keepDownloaded, !isDirectory, !isDownloaded {
            try await manager.requestDownloadForItem(withIdentifier: identifier)
        } else {
            try await manager.requestModification(
                of: [.lastUsedDate], forItemWithIdentifier: identifier
            )
        }
    }

    ///
    /// Populate the local database with every descendant of this directory by
    /// walking the remote tree breadth-first, issuing one depth-1 PROPFIND per
    /// directory encountered.
    ///
    /// This replaces a single infinite-depth PROPFIND, which
    /// `Enumerator.readServerUrl` explicitly rejects because it can pull the
    /// entire subtree into one response and overload the server on large
    /// directory hierarchies.
    ///
    /// Failures on individual descendants are logged and skipped so that one
    /// broken branch does not abort pinning of the rest. A failure on the
    /// top-level target is propagated — the user explicitly asked to pin this
    /// item, so it must succeed there.
    ///
    private func enumerateSubtreeBreadthFirst(domain: NSFileProviderDomain) async throws {
        var remoteDirectoryPaths: [String] = [metadata.remotePath()]
        var isTopLevel = true

        while !remoteDirectoryPaths.isEmpty {
            let remoteDirectoryPath = remoteDirectoryPaths.removeFirst()

            let (metadatas, _, _, _, _, readError) = await Enumerator.readServerUrl(
                remoteDirectoryPath,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                domain: domain,
                enumeratedItemIdentifier: itemIdentifier,
                log: logger.log
            )

            if let readError, readError != .success {
                if isTopLevel {
                    logger.error(
                        "Could not enumerate directory for keep-downloaded.",
                        [.name: metadata.fileName, .url: remoteDirectoryPath, .error: readError]
                    )
                    throw readError.fileProviderError(
                        handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier
                    ) ?? NSFileProviderError(.cannotSynchronize)
                } else {
                    // A single failing descendant must not abort the whole
                    // subtree — log and skip the rest of this branch.
                    logger.error(
                        "Could not enumerate descendant directory for keep-downloaded; skipping branch.",
                        [.url: remoteDirectoryPath, .error: readError]
                    )
                    isTopLevel = false
                    continue
                }
            }

            isTopLevel = false

            guard var metadatas else { continue }

            // `readServerUrl` returns the target directory as the first entry
            // for depth-1 reads; drop it before queueing children.
            if !metadatas.isEmpty {
                metadatas.removeFirst()
            }

            for child in metadatas where child.directory {
                remoteDirectoryPaths.append(child.remotePath())
            }
        }
    }
}

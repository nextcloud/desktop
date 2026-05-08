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
            if #available(macOS 14.1, *) {
                throw NSFileProviderError(.providerDomainNotFound)
            } else {
                let providerDomainNotFoundErrorCode = -2013
                throw NSError(domain: NSFileProviderErrorDomain, code: providerDomainNotFoundErrorCode, userInfo: [NSLocalizedDescriptionKey: "Failed to get manager for domain."])
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

        try await signalKeepDownloaded(keepDownloaded: keepDownloaded, identifier: itemIdentifier, isDirectory: metadata.directory, isDownloaded: isDownloaded, manager: manager)

        // When disabling, fragment the chain of pinned ancestors above this
        // item so the unpin actually takes effect against
        // `.downloadEagerlyAndKeepDownloaded` inheritance (#9891). Siblings at
        // every level retain their pin via the flag already set on them by
        // the original recursive enable; we only need to flip the path
        // ancestors themselves to `.inherited`.
        if !keepDownloaded {
            try await fragmentPathToRoot(manager: manager)
        }

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
    private func signalKeepDownloaded(keepDownloaded: Bool, identifier: NSFileProviderItemIdentifier, isDirectory: Bool, isDownloaded: Bool, manager: NSFileProviderManager) async throws {
        if keepDownloaded, !isDirectory, !isDownloaded {
            try await manager.requestDownloadForItem(withIdentifier: identifier)
        } else {
            try await manager.requestModification(of: [.lastUsedDate], forItemWithIdentifier: identifier)
        }
    }

    ///
    /// Walk the parent chain of this item, flipping every pinned ancestor's
    /// keep-downloaded flag to `false` so its `contentPolicy` reverts from
    /// `.downloadEagerlyAndKeepDownloaded` to `.inherited`.
    ///
    /// Why: the framework refuses `evictItem` on items whose effective
    /// content policy is `.downloadEagerlyAndKeepDownloaded`, and that policy
    /// inherits down the tree. Recursive "Always keep downloaded" sets the
    /// strict policy on every ancestor; unpinning a deep descendant
    /// in isolation leaves the descendant's `keepDownloaded == false` but its
    /// effective policy unchanged because some ancestor still resolves to the
    /// strict value. To actually free the descendant we have to cut the
    /// ancestors out of the strict-pin chain. Siblings at each level retain
    /// their pin via the flag already set on them by the original recursive
    /// enable, so no sibling write is needed (#9891).
    ///
    /// The walk stops at the first ancestor that is not pinned; if no pinned
    /// ancestor exists, this is a no-op (the unpin gesture targets the pin
    /// root itself or there was no pin tree).
    ///
    private func fragmentPathToRoot(manager: NSFileProviderManager) async throws {
        let outcome = fragmentPathToRootInDatabase()

        // Cousins newly transitioned from `.inherited` (under a strict-pinned
        // ancestor) to an explicit pin must be told to the framework so it
        // refreshes their `contentPolicy` to `.downloadEagerlyAndKeepDownloaded`
        // — otherwise the unpin of the path ancestors would silently take
        // them down with the path.
        for cousin in outcome.newlyPinnedCousins {
            do {
                try await signalKeepDownloaded(keepDownloaded: true, identifier: NSFileProviderItemIdentifier(cousin.ocId), isDirectory: cousin.directory, isDownloaded: cousin.downloaded, manager: manager)
            } catch {
                logger.error("Could not signal newly-pinned cousin during unpin-path fragmentation.", [.item: cousin.ocId, .name: cousin.fileName, .error: error])
            }
        }

        // Apply ancestor flips top-down (pin root first). Order does not affect
        // correctness — each ancestor is independent — but matches the
        // conceptual order of "cut from the top of the strict-pin chain
        // downward".
        for ancestor in outcome.unpinnedAncestors.reversed() {
            do {
                try await signalKeepDownloaded(keepDownloaded: false, identifier: NSFileProviderItemIdentifier(ancestor.ocId), isDirectory: ancestor.directory, isDownloaded: ancestor.downloaded, manager: manager)
            } catch {
                logger.error("Could not signal fragmented ancestor unpin to framework.", [.item: ancestor.ocId, .name: ancestor.fileName, .error: error])
            }
        }
    }

    ///
    /// Outcome of a database-only fragmentation walk.
    ///
    /// `unpinnedAncestors` are listed bottom-up (immediate parent first, pin
    /// root last) — the order in which the walk discovered them. Callers that
    /// signal the framework should emit the unpin events top-down to mirror
    /// the conceptual "cut from the top of the strict-pin chain downward".
    ///
    internal struct FragmentationOutcome {
        let unpinnedAncestors: [SendableItemMetadata]
        let newlyPinnedCousins: [SendableItemMetadata]
    }

    ///
    /// Database-only counterpart to ``fragmentPathToRoot(manager:)``.
    /// Walks parents, pins every off-path immediate child ("cousin") that
    /// isn't already pinned, then flips every pinned ancestor's flag to
    /// `false`. The framework-side signaling is the caller's job.
    ///
    /// Why pin cousins explicitly instead of trusting the original recursive
    /// enable: the recursive enable runs once at pin time and only sees
    /// items the database knew about then. Items discovered later — e.g. a
    /// new server-side sibling surfaced by enumeration — enter with
    /// `keepDownloaded == false`. Without this explicit re-pin those cousins
    /// would silently lose their pin when the path ancestors flip to
    /// `.inherited` (#9891).
    ///
    /// Exposed at module scope so tests can verify the flag mutations
    /// without needing a registered file provider domain.
    ///
    internal func fragmentPathToRootInDatabase() -> FragmentationOutcome {
        // (ancestor, ocId of the child along the path) bottom-up.
        var pinnedAncestorsAndPathChildren: [(ancestor: SendableItemMetadata, pathChildOcId: String)] = []
        var cursor = metadata

        while let parent = dbManager.parentDirectoryMetadataForItem(cursor) {
            guard parent.keepDownloaded else {
                break
            }

            pinnedAncestorsAndPathChildren.append((ancestor: parent, pathChildOcId: cursor.ocId))
            cursor = parent
        }

        var newlyPinnedCousins: [SendableItemMetadata] = []

        for (ancestor, pathChildOcId) in pinnedAncestorsAndPathChildren {
            // Pin every immediate child of this ancestor except the one along
            // the path. Cousins already flagged are skipped to avoid noisy
            // DB writes (and noisy framework signals downstream).
            for cousin in dbManager.immediateChildItems(directoryMetadata: ancestor) where cousin.ocId != pathChildOcId && !cousin.keepDownloaded {
                do {
                    _ = try dbManager.set(keepDownloaded: true, for: cousin)
                    newlyPinnedCousins.append(cousin)
                } catch {
                    logger.error("Could not pin off-path sibling while fragmenting unpin path.", [.item: cousin.ocId, .name: cousin.fileName, .error: error])
                }
            }

            // Then flip the ancestor itself.
            do {
                _ = try dbManager.set(keepDownloaded: false, for: ancestor)
            } catch {
                logger.error("Could not clear keep-downloaded on ancestor while fragmenting unpin path.", [.item: ancestor.ocId, .name: ancestor.fileName, .error: error])
                continue
            }
        }

        return FragmentationOutcome(unpinnedAncestors: pinnedAncestorsAndPathChildren.map(\.ancestor), newlyPinnedCousins: newlyPinnedCousins)
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
                    logger.error("Could not enumerate directory for keep-downloaded.", [.name: metadata.fileName, .url: remoteDirectoryPath, .error: readError])
                    throw readError.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier) ?? NSFileProviderError(.cannotSynchronize)
                } else {
                    // A single failing descendant must not abort the whole
                    // subtree — log and skip the rest of this branch.
                    logger.error("Could not enumerate descendant directory for keep-downloaded; skipping branch.", [.url: remoteDirectoryPath, .error: readError])
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

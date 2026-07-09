//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider

///
/// Refresh the framework's cached snapshot of every ancestor container of the
/// given files — up to and including the root container — so their "Remove
/// download" (`displayEvict`) visibility updates on the whole path (#10085).
///
/// A container's `displayEvict` depends on whether it holds a materialized
/// descendant file, which lives outside the container's own etag, so the
/// framework must be nudged with `requestModification(of: [.lastUsedDate], …)`
/// to re-pull the item; the container's `metadataVersion` folds in the same
/// descendant state (see ``Item/itemVersion``) so the re-pull is not
/// deduplicated away. This is the same nudge ``Item/signalKeepDownloaded`` and
/// the extension-version cache refresh use. Fire and forget.
///
/// It must be invoked from **every** site where a file's `downloaded` flag
/// flips, because they use different code paths:
/// - ``Item/fetchContents(domain:progress:dbManager:)`` (a download) writes
///   `downloaded = true` to the database *before* the system re-enumerates its
///   materialized set, so the ``MaterializedEnumerationObserver`` reconciliation
///   sees no discrepancy and would not otherwise fire here.
/// - The observer covers eviction (a file going dataless) and out-of-band
///   materialization it discovers itself.
///
func refreshRemoveDownloadVisibility(
    forAncestorsOfFileOcIds ocIds: Set<String>,
    manager: NSFileProviderManager,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    guard !ocIds.isEmpty else { return }

    // Everything — the ancestor walk (a synchronous Realm read) and the nudges —
    // runs inside the Task so callers on latency-sensitive paths (the
    // materialized-set completion handler, `fetchContents`) are never blocked.
    Task {
        let ancestors = dbManager.ancestorContainerIdentifiers(ofFileItemsWithOcIds: ocIds)

        guard !ancestors.isEmpty else { return }

        logger.debug("Refreshing \(ancestors.count) ancestor container(s) to update Remove download visibility after materialization change.")

        for ancestor in ancestors {
            do {
                try await manager.requestModification(of: [.lastUsedDate], forItemWithIdentifier: ancestor)
            } catch {
                logger.error("Could not nudge ancestor container to refresh Remove download visibility.", [.item: ancestor, .error: error.localizedDescription])
            }
        }
    }
}

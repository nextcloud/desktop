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
/// Batched by ``AncestorRefreshCoalescer``, so callers only take a short lock.
///
func refreshRemoveDownloadVisibility(
    forAncestorsOfFileOcIds ocIds: Set<String>,
    domain: NSFileProviderDomainIdentifier,
    manager: NSFileProviderManager,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    guard !ocIds.isEmpty else { return }

    AncestorRefreshCoalescer.shared(for: domain).enqueue(
        ocIds: ocIds,
        ancestors: { dbManager.ancestorContainerIdentifiers(ofFileItemsWithOcIds: $0) },
        nudge: { try await manager.requestModification(of: [.lastUsedDate], forItemWithIdentifier: $0) },
        log: logger.log
    )
}

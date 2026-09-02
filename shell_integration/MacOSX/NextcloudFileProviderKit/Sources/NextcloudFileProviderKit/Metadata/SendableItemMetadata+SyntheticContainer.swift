//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// Stand-in timestamp for a synthetic container (the root and trash containers) that has no
/// persisted row yet.
///
/// It must be a **constant**, not `Date()`. ``Item/rootContainer(account:remoteInterface:dbManager:remoteSupportsTrash:log:)``
/// and ``Item/trashContainer(remoteInterface:account:dbManager:remoteSupportsTrash:log:)`` build
/// their metadata from scratch on every call, so a wall-clock default made every read of the
/// container return new `creationDate` / `contentModificationDate` / `lastUsedDate` values. The
/// framework compared those against its cached snapshot, logged
/// `diffs:lastUsedDate|btime|mtime why:item propagated`, and re-queued the container's
/// `update-item` job — which could never converge because the next read moved the timestamps
/// again. Observed as the root container being updated 13 times in 93 seconds under a single
/// scheduler ID during a bulk materialisation.
///
let syntheticContainerFallbackDate = Date(timeIntervalSince1970: 0)

extension SendableItemMetadata {
    ///
    /// Overlay the persisted database row for a synthetic container onto freshly synthesised
    /// metadata.
    ///
    /// Two kinds of state have to survive the synthesis:
    ///
    /// - Per-item toggles (`keepDownloaded`, `downloaded`). Without them the container is always
    ///   rebuilt with defaults and `userInfo` keeps offering "Always keep downloaded" even after
    ///   the user enabled it, because `displayKeepDownloaded` / `displayAllowAutoEvicting` and
    ///   `contentPolicy` all derive from the synthesised metadata.
    /// - Identity that the framework diffs against its cached snapshot (`creationDate`, `date`,
    ///   `etag`). The server's real values are already stored — `NKFile.toItemMetadata()` writes
    ///   them and remaps the server root row's `ocId` to `NSFileProviderItemIdentifier.rootContainer`
    ///   — so reading them back is what makes the container's item stable between calls. `date`
    ///   backs both `contentModificationDate` and `lastUsedDate`.
    ///
    /// `etag` is included deliberately: a synthesised container carries `etag: ""`, which leaves
    /// its `contentVersion` empty and its `metadataVersion` varying only with the bundle version.
    /// Carrying the real etag lets the framework detect that the container actually converged.
    /// Containers have no content to download, so a changing `contentVersion` costs nothing here.
    ///
    mutating func mergePersistedSyntheticContainerState(dbManager: FilesDatabaseManager) {
        guard let existing = dbManager.itemMetadata(ocId: ocId) else { return }

        keepDownloaded = existing.keepDownloaded
        downloaded = existing.downloaded
        creationDate = existing.creationDate
        date = existing.date
        etag = existing.etag
    }
}

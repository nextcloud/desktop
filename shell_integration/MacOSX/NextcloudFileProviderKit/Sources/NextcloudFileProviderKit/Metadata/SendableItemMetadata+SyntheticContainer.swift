//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// Stand-in timestamp for a synthetic container (the root and trash containers) that has no
/// persisted row yet, a **constant** rather than `Date()` because the containers are rebuilt from
/// scratch on every call and moving timestamps re-queue their `update-item` job forever.
///
let syntheticContainerFallbackDate = Date(timeIntervalSince1970: 0)

extension SendableItemMetadata {
    ///
    /// Overlay the persisted row for a synthetic container onto freshly synthesised metadata, so
    /// the per-item toggles and the identity the framework diffs against its cached snapshot both
    /// survive the synthesis.
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

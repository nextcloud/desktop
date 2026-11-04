//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

///
/// What one working-set scan found on the server.
///
struct WorkingSetScanResult {
    /// The discovered creations and updates.
    var updated: [SendableItemMetadata]

    /// The items that were marked deleted.
    var deleted: [SendableItemMetadata]

    /// Whether anything could not be read, so the sync point must not advance past it.
    var hadFailure: Bool

    /// The items whose read failed.
    var failedOcIds: Set<String>

    /// Whether the materialised items could not be read from the database.
    var materialisedItemsUnreadable: Bool

    /// Whether an item was not found and no read in this scan showed whether it was deleted or moved.
    var needsFullWalk: Bool
}

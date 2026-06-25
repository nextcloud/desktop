//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// A set of item changes derived by comparing a fresh server listing against the local database.
///
/// Produced by the change-deriving database read (``FilesDatabaseManager/depth1ReadUpdateItemMetadatas(account:serverUrl:updatedMetadatas:keepExistingDownloadState:)``)
/// and carried through the enumerator to the file-provider change observer. Replaces the three
/// parallel `newMetadatas` / `updatedMetadatas` / `deletedMetadatas` arrays that used to be passed
/// around as a positional tuple.
///
public struct ChangeSet: Sendable {
    ///
    /// Items that did not previously exist in the local database.
    ///
    public var created: [SendableItemMetadata]

    ///
    /// Items already known locally whose remotely-stored state changed.
    ///
    public var updated: [SendableItemMetadata]

    ///
    /// Items that were present locally but are gone from the server listing.
    ///
    public var deleted: [SendableItemMetadata]

    public init(
        created: [SendableItemMetadata] = [],
        updated: [SendableItemMetadata] = [],
        deleted: [SendableItemMetadata] = []
    ) {
        self.created = created
        self.updated = updated
        self.deleted = deleted
    }

    ///
    /// Combine update and deletion lists gathered from several sources into one de-duplicated report.
    ///
    /// Used by the working-set change enumeration to fold the changes discovered directly against the
    /// server together with the pending local changes re-derived from the database. Deletions take
    /// precedence: any ocId present in a deletion list is never also reported as an update. Within
    /// the updates and within the deletions, the first occurrence of each ocId wins. The resulting
    /// ``created`` list is always empty — every surviving item is reported as an update.
    ///
    /// - Parameters:
    ///   - updatedLists: Update contributions, in priority order (earlier lists win on ocId ties).
    ///   - deletedLists: Deletion contributions, in priority order.
    ///
    public init(mergingUpdated updatedLists: [[SendableItemMetadata]], deleted deletedLists: [[SendableItemMetadata]]) {
        let deletedById = Dictionary(
            deletedLists.flatMap(\.self).map { ($0.ocId, $0) },
            uniquingKeysWith: { first, _ in first }
        )
        var seenUpdates = Set<String>()
        let mergedUpdates = updatedLists.flatMap(\.self).filter { metadata in
            guard deletedById[metadata.ocId] == nil else { return false }
            return seenUpdates.insert(metadata.ocId).inserted
        }
        self.init(created: [], updated: mergedUpdates, deleted: Array(deletedById.values))
    }

    ///
    /// Whether no change of any kind was found.
    ///
    public var isEmpty: Bool {
        created.isEmpty && updated.isEmpty && deleted.isEmpty
    }

    ///
    /// Creations and updates combined, creations first.
    ///
    /// The file-provider change observer does not distinguish newly-created from modified items —
    /// both are reported through `didUpdate(_:)` — so reporting code consumes this combined view.
    ///
    public var createdAndUpdated: [SendableItemMetadata] {
        created + updated
    }
}

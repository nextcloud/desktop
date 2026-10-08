//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// What the working-set scan remembers between passes and asks while it walks.
///
/// `scannedRemoteState` is the etag and permissions of a directory, as its parent's listing reports them, at the moment the scan had reconciled everything materialized below it. The parent's listing is the view the walk decides by, and it is not the directory's own: the server rewrites a mount point's permissions in the parent's listing (`View::getDirectoryContent()` adds or removes update and delete for mount points), so a state taken from the directory's own listing never matches for shares, group folders and app mounts. The state is written by the scan alone: every other writer of a directory's etag (item pages, action replies, the materialized-item mirror) stores the etag without looking below the directory, so the stored etag cannot stand in for "subtree reconciled". A listing which fails forgets the state of the directory and its ancestors, so the next pass descends there again.
///
extension FilesDatabaseManager {
    /// The mount type the server reports for every item on an external storage.
    static let externalMountType = "external"

    /// The remote state the working-set scan compares a directory against, built from what its parent's listing reports.
    static func scanRemoteState(of metadata: SendableItemMetadata) -> String {
        "\(metadata.etag)|\(metadata.permissions)"
    }

    /// The remembered states of every directory the scan reconciled, keyed by identifier, or `nil` when the read fails.
    func scannedRemoteStates() -> [String: String]? {
        read("Could not read which directories the working-set scan reconciled.") { db in
            var states: [String: String] = [:]
            let rows = try Row.fetchCursor(
                db,
                sql: "SELECT ocId, scannedRemoteState FROM itemMetadata WHERE directory = 1 AND scannedRemoteState IS NOT NULL"
            )

            while let row = try rows.next() {
                states[row[0]] = row[1]
            }

            return states
        }
    }

    ///
    /// Remember the directories a pass reconciled and forget the ones whose listing failed, in one relaxed write.
    ///
    /// A lost write costs one extra pass over those directories, never state, so the write is relaxed.
    ///
    func recordScannedRemoteStates(_ states: [String: String], forgetting forgottenOcIds: Set<String>) {
        guard !states.isEmpty || !forgottenOcIds.isEmpty else {
            return
        }

        write("Could not record which directories the working-set scan reconciled.", durability: .relaxed) { db in
            for chunk in Array(forgottenOcIds).chunked(into: Self.inClauseChunkSize) {
                try ItemMetadataRecord
                    .filter(chunk.contains(ItemMetadataRecord.Columns.ocId))
                    .updateAll(db, ItemMetadataRecord.scannedRemoteStateColumn.set(to: nil))
            }

            let statement = try db.cachedStatement(sql: "UPDATE itemMetadata SET scannedRemoteState = ? WHERE ocId = ?")
            for (ocId, state) in states where !forgottenOcIds.contains(ocId) {
                try statement.execute(arguments: [state, ocId])
            }
        }
    }

    /// Whether a live materialized item is stored anywhere below the directory at `remotePath`, or `nil` when the read fails.
    func hasMaterialisedItem(below remotePath: String) -> Bool? {
        read("Could not look for materialized items below a directory.", [.url: remotePath]) { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.hasServerUrl(equalTo: remotePath, includingDescendants: true)
                        && ItemMetadataRecord.isMaterialised
                        && ItemMetadataRecord.Columns.deleted == false
                )
                .limit(1)
                .fetchCount(db) > 0
        }
    }

    /// Whether a live row of an external storage is stored anywhere below the directory at `remotePath`, or `nil` when the read fails. Every item inside such a mount carries the mount type, so a visited directory or a downloaded file inside one is found.
    func hasExternalStorage(below remotePath: String) -> Bool? {
        read("Could not look for external storages below a directory.", [.url: remotePath]) { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.hasServerUrl(equalTo: remotePath, includingDescendants: true)
                        && ItemMetadataRecord.Columns.mountType == Self.externalMountType
                        && ItemMetadataRecord.Columns.deleted == false
                )
                .limit(1)
                .fetchCount(db) > 0
        }
    }

    /// Whether a live row with another identifier occupies the logical address, as after a delete and re-create under the same name, or `nil` when the read fails.
    func liveItemOccupies(serverUrl: String, fileName: String, otherThan ocId: String) -> Bool? {
        read("Could not look up the occupant of a logical address.", [.url: serverUrl, .name: fileName]) { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.hasLocation(serverUrl: serverUrl, fileName: fileName)
                        && ItemMetadataRecord.Columns.deleted == false
                        && ItemMetadataRecord.Columns.ocId != ocId
                )
                .limit(1)
                .fetchCount(db) > 0
        }
    }
}

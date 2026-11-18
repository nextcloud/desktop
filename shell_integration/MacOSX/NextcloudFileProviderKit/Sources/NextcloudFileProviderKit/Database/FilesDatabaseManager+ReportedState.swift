//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Which row state the system has already received.
///
/// Every row carries the `syncTime` of the state last handed to the File Provider framework in `reportedSyncTime`. The working-set derivation reports a row only while its `syncTime` is newer than that mark, so state the system received through an item enumeration page or an action reply is not reported again as a working-set change.
///
/// A mark is the handed state's own `syncTime`, never the clock: a write which lands between reading the state and marking it stamps a newer `syncTime` and stays pending. It never exceeds the row's stored `syncTime`, because `syncTime` is stamped when a value is built and some writers store a value built earlier. It is only ever raised, so hand-overs may race in any order.
///
extension FilesDatabaseManager {
    /// Raises the mark of one row to the handed `syncTime`, capped at the row's stored `syncTime`.
    static let markReportedSQL = """
    UPDATE \(ItemMetadataRecord.databaseTableName)
    SET reportedSyncTime = MIN(?, syncTime)
    WHERE ocId = ? AND (reportedSyncTime IS NULL OR reportedSyncTime < MIN(?, syncTime))
    """

    ///
    /// Record that the system received the state carried by `metadatas`.
    ///
    /// Rows which are unknown, or already marked at or after the handed state, are left alone. The write is relaxed: a mark lost to a crash costs one redundant report, never state.
    ///
    public func markReported(_ metadatas: [SendableItemMetadata]) {
        guard !metadatas.isEmpty else {
            return
        }

        write("Could not record the reported item state.", [.name: "\(metadatas.count) item(s)"], durability: .relaxed) { db in
            try markReported(metadatas, in: db)
        }
    }

    func markReported(_ metadatas: [SendableItemMetadata], in db: Database) throws {
        let statement = try db.cachedStatement(sql: Self.markReportedSQL)

        for metadata in metadatas {
            let syncTime = metadata.syncTime.timeIntervalSinceReferenceDate
            try statement.execute(arguments: [syncTime, metadata.ocId, syncTime])
        }
    }

    /// Moves a mark which covered the row's previous state up to the rewritten state. Only rows marked at or after `unchangedSince` qualify.
    private static let carryReportedMarkSQL = """
    UPDATE \(ItemMetadataRecord.databaseTableName)
    SET reportedSyncTime = MIN(?, syncTime)
    WHERE ocId = ? AND reportedSyncTime >= ?
    """

    ///
    /// Keep a row reported across a rewrite which changed nothing the system sees.
    ///
    /// `unchangedSince` is the `syncTime` of the previous row state. When the system had that state, it has the rewritten one too, so the mark follows the new `syncTime`. A row the system never had, or had only in an older state, stays pending.
    ///
    func carryReportedMark(of metadata: SendableItemMetadata, unchangedSince: Date, in db: Database) throws {
        try db.cachedStatement(sql: Self.carryReportedMarkSQL).execute(arguments: [
            metadata.syncTime.timeIntervalSinceReferenceDate,
            metadata.ocId,
            unchangedSince.timeIntervalSinceReferenceDate
        ])
    }

    ///
    /// The subset of `metadatas` whose row does not carry a mark newer than the item's own `syncTime`.
    ///
    /// Used right before a buffered change batch is handed over, so a snapshot older than state the system received since is dropped and the newer state is not overwritten. Equal stamps are kept. When the read fails, everything is kept.
    ///
    func notYetReported(_ metadatas: [SendableItemMetadata]) -> [SendableItemMetadata] {
        guard !metadatas.isEmpty else {
            return metadatas
        }

        let marks: [String: Double]? = read("Could not read the reported item state.", [.name: "\(metadatas.count) item(s)"]) { db in
            var marks: [String: Double] = [:]

            for chunk in metadatas.map(\.ocId).chunked(into: Self.inClauseChunkSize) {
                let rows = try Row.fetchAll(
                    db,
                    ItemMetadataRecord
                        .filter(chunk.contains(ItemMetadataRecord.Columns.ocId) && ItemMetadataRecord.reportedSyncTimeColumn != nil)
                        .select(ItemMetadataRecord.Columns.ocId, ItemMetadataRecord.reportedSyncTimeColumn)
                )

                for row in rows {
                    marks[row[0]] = row[1]
                }
            }

            return marks
        }

        guard let marks else {
            return metadatas
        }

        return metadatas.filter { metadata in
            guard let mark = marks[metadata.ocId] else {
                return true
            }

            return mark <= metadata.syncTime.timeIntervalSinceReferenceDate
        }
    }
}

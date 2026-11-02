//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Fetching item rows one by one, so a row which cannot be decoded is skipped and logged instead of failing the whole result.
///
/// A skipped row behaves like a missing row: the next write of the same item replaces it, which is how such a row heals. With the current schema SQLite coerces mistyped scalars and the list columns decode leniently, so this is the safety net for structural damage rather than an expected path.
///
extension QueryInterfaceRequest<ItemMetadataRecord> {
    /// Every decodable row of the request.
    func fetchRecords(_ db: Database, logger: FileProviderLogger) throws -> [ItemMetadataRecord] {
        var records: [ItemMetadataRecord] = []
        let rows = try Row.fetchCursor(db, self)

        while let row = try rows.next() {
            if let record = ItemMetadataRecord.decode(row, logger: logger) {
                records.append(record)
            }
        }

        return records
    }

    /// The first decodable row of the request.
    func fetchRecord(_ db: Database, logger: FileProviderLogger) throws -> ItemMetadataRecord? {
        let rows = try Row.fetchCursor(db, self)

        while let row = try rows.next() {
            if let record = ItemMetadataRecord.decode(row, logger: logger) {
                return record
            }
        }

        return nil
    }
}

extension ItemMetadataRecord {
    /// The record for the row, or `nil` after logging why it cannot be decoded. Array columns which decoded as empty are logged as well.
    static func decode(_ row: Row, logger: FileProviderLogger) -> ItemMetadataRecord? {
        let ocId = try? row.decode(String?.self, forColumn: "ocId")

        do {
            let record = try ItemMetadataRecord(row: row)

            for column in record.unreadableColumns {
                logger.error("A stored list of an item could not be decoded and is treated as empty.", [.item: ocId, .name: column])
            }

            return record
        } catch {
            logger.fault("Skipping a stored item which cannot be decoded.", [.item: ocId, .error: error])
            return nil
        }
    }
}

//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

/// The columns the startup repair of logical addresses walks, so the whole table need not be loaded.
struct ItemLogicalAddressRow: Codable, Sendable, FetchableRecord, TableRecord {
    static let databaseTableName = ItemMetadataRecord.databaseTableName
    static var databaseSelection: [any SQLSelectable] {
        [
            Column("davRoot"),
            Column("deleted"),
            Column("fileName"),
            Column("isLockFileOfLocalOrigin"),
            Column("normalizedFileName"),
            Column("normalizedParentPath"),
            Column("ocId"),
            Column("parentPath"),
            Column("status"),
            Column("syncTime")
        ]
    }

    static func databaseDateDecodingStrategy(for _: String) -> DatabaseDateDecodingStrategy {
        .timeIntervalSinceReferenceDate
    }

    var ocId: String
    var davRoot: String
    var parentPath: String
    var fileName: String
    var normalizedParentPath: String
    var normalizedFileName: String
    var deleted: Bool
    var isLockFileOfLocalOrigin: Bool
    var status: Int
    var syncTime: Date
}

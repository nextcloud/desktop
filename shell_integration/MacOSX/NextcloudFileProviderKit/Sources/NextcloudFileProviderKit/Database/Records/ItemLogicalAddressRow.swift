//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

/// The columns the startup repair of logical addresses walks, so the whole table need not be loaded.
struct ItemLogicalAddressRow: Codable, Sendable, FetchableRecord, TableRecord {
    static let databaseTableName = ItemMetadataRecord.databaseTableName
    static var databaseSelection: [any SQLSelectable] {
        [
            Column("ocId"), Column("serverUrl"), Column("fileName"), Column("normalizedServerUrl"),
            Column("normalizedFileName"), Column("deleted"), Column("isLockFileOfLocalOrigin"), Column("status"),
            Column("syncTime")
        ]
    }

    static func databaseDateDecodingStrategy(for _: String) -> DatabaseDateDecodingStrategy {
        .timeIntervalSinceReferenceDate
    }

    var ocId: String
    var serverUrl: String
    var fileName: String
    var normalizedServerUrl: String
    var normalizedFileName: String
    var deleted: Bool
    var isLockFileOfLocalOrigin: Bool
    var status: Int
    var syncTime: Date
}

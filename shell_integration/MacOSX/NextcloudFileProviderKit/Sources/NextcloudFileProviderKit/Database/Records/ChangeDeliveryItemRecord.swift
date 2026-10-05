//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

/// One durably stored item of a multi-batch File Provider change enumeration.
struct ChangeDeliveryItemRecord: Codable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "changeDeliveryItem"

    typealias Columns = CodingKeys

    enum CodingKeys: String, CodingKey, ColumnExpression {
        case sessionId, sequence, metadataData, deleted
    }

    var sessionId: String
    var sequence: Int
    var metadataData: Data
    var deleted: Bool
}

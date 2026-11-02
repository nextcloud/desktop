//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

/// The single row naming the database file: a random identity minted when the file is first opened.
struct StoreIdentityRecord: Codable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "storeIdentity"

    var id = 1
    var identity: String

    /// The file's identity, minting one when the row is missing.
    static func ensure(in db: Database) throws -> String {
        if let record = try fetchOne(db) {
            return record.identity
        }

        let record = StoreIdentityRecord(identity: UUID().uuidString)
        try record.insert(db)
        return record.identity
    }
}

// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

struct PendingCreateUploadRecord: Codable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "pendingCreateUpload"
    static let retention: TimeInterval = 7 * 24 * 60 * 60

    let account: String
    let itemIdentifier: String
    let remotePath: String
    let checksum: String
    let attemptIdentifier: String
    let startedAt: TimeInterval

    init(account: String, itemIdentifier: String, remotePath: String, checksum: String) {
        self.account = account
        self.itemIdentifier = itemIdentifier
        self.remotePath = remotePath
        self.checksum = checksum
        attemptIdentifier = UUID().uuidString
        startedAt = Date().timeIntervalSince1970
    }
}

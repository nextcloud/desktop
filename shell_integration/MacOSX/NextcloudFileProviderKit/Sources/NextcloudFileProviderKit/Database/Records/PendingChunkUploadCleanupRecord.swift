//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import GRDB

/// Row of a chunk upload whose local cleanup must be retried.
struct PendingChunkUploadCleanupRecord: Codable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "pendingChunkUploadCleanup"

    var uploadIdentifier: String
}

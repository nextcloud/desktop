//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import GRDB

/// Row of an item awaiting the deletion callback triggered by `.excludedFromSync`.
struct ExcludedFromSyncItemRecord: Codable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "excludedFromSyncItem"

    var ocId: String
}

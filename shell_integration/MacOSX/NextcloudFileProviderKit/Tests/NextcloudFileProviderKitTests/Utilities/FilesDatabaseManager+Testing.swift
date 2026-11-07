//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import RealmSwift

extension FilesDatabaseManager {
    /// Store a row exactly as given, bypassing duplicate eviction and the inheritance of local flags.
    func insertForTesting(_ metadata: SendableItemMetadata) throws {
        let database = ncDatabase()

        try database.write {
            database.add(RealmItemMetadata(value: metadata), update: .all)
        }
    }
}

//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import RealmSwift

/** @brief Durable record of an item awaiting the deletion callback triggered by `.excludedFromSync`. */
final class RealmExcludedFromSyncItem: Object {
    /** @brief The file provider item identifier associated with the exclusion. */
    @Persisted(primaryKey: true) var ocId = ""

    /** @brief Creates an exclusion record for the provided item identifier. */
    convenience init(ocId: String) {
        self.init()
        self.ocId = ocId
    }
}

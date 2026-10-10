//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation // `@objc(...)` below needs it in scope.
import RealmSwift

/** @brief Durable record of a chunk upload whose local cleanup must be retried. */
@objc(RealmPendingChunkUploadCleanup)
final class LegacyRealmPendingChunkUploadCleanup: Object {
    /** @brief The chunk-upload identifier used to locate the local chunks. */
    @Persisted(primaryKey: true) var uploadIdentifier = ""

    /** @brief Creates a cleanup record for the provided chunk-upload identifier. */
    convenience init(uploadIdentifier: String) {
        self.init()
        self.uploadIdentifier = uploadIdentifier
    }
}

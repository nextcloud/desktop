//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

extension ChangeDeliveryItemRecord {
    /// Convert queued item JSON from Realm to the SQLite format, which derives the path from the parent URL.
    /// Return `nil` if the item cannot be decoded so the caller can keep the original JSON.
    static func migrateLegacySnapshot(_ data: Data) throws -> Data? {
        guard let metadata = try? decodeMetadata(data) else { return nil }
        return try encodeMetadata(metadata)
    }
}

//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudKit

extension ChangeDeliveryItemRecord {
    /// Convert queued item JSON from Realm to the SQLite format, which derives the path from the parent URL.
    /// Keep the original JSON if the snapshot cannot be decoded or has no recognized DAV root.
    static func migrateLegacySnapshot(_ data: Data) throws -> Data? {
        guard let metadata = try? decodeMetadata(data) else { return nil }
        do {
            return try encodeMetadata(metadata)
        } catch let error as NKError where error == .invalidData {
            return nil
        }
    }
}

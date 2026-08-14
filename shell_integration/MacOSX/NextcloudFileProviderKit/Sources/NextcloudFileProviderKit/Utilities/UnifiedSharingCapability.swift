//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

enum UnifiedSharingCapability {
    static func isAvailable(in responseData: Data?) -> Bool {
        guard let responseData,
              let root = try? JSONSerialization.jsonObject(with: responseData) as? [String: Any],
              let ocs = root["ocs"] as? [String: Any],
              let data = ocs["data"] as? [String: Any],
              let capabilities = data["capabilities"] as? [String: Any]
        else {
            return false
        }

        return capabilities.keys.contains("sharing")
    }
}

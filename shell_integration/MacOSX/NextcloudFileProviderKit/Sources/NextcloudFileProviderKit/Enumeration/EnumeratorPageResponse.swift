//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
import Foundation

struct EnumeratorPageResponse: Sendable, Codable {
    let token: String? // Required by server to serve the next page of items
    let index: Int // Needed to calculate the offset for the next paginated request
    var total: Int? // Total item count, provided in the first non-offset paginated response

    init?(nkResponseData: AFDataResponse<Data>?, index: Int, log _: any FileProviderLogging) {
        guard let headers = nkResponseData?.response?.allHeaderFields as? [String: String] else {
            return nil
        }

        let normalisedHeaders =
            Dictionary(uniqueKeysWithValues: headers.map { ($0.key.lowercased(), $0.value) })
        guard Bool(normalisedHeaders["x-nc-paginate"]?.lowercased() ?? "false") == true,
              let responsePaginateToken = normalisedHeaders["x-nc-paginate-token"]
        else {
            return nil
        }

        self.index = index
        token = responsePaginateToken

        if let responsePaginateTotal = normalisedHeaders["x-nc-paginate-total"] {
            total = Int(responsePaginateTotal)
        } else {
            total = nil
        }
    }
}

//
//  EnumeratorPageResponse.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 16/5/25.
//

import Alamofire
import Foundation

struct EnumeratorPageResponse: Sendable {
    let token: String   // Required by server to serve the next page of items
    let index: Int      // Needed to calculate the offset for the next paginated request
    let total: Int?     // Total item count, provided in the first non-offset paginated response

    init?(nkResponseData: AFDataResponse<Data>?, index: Int) {
        guard let headers = nkResponseData?.response?.allHeaderFields as? [String: String] else {
            return nil
        }

        let normalisedHeaders =
            Dictionary(uniqueKeysWithValues: headers.map { ($0.key.lowercased(), $0.value) })
        print(normalisedHeaders)
        guard Bool(normalisedHeaders["x-nc-paginate"]?.lowercased() ?? "false") == true,
              let responsePaginateToken = normalisedHeaders["x-nc-paginate-token"]
        else { return nil }

        self.index = index
        token = responsePaginateToken
        if let responsePaginateTotal = normalisedHeaders["x-nc-paginate-total"] {
            total = Int(responsePaginateTotal)
        } else {
            total = nil
        }
    }
}

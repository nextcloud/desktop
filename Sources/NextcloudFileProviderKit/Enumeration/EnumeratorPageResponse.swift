//
//  EnumeratorPageResponse.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 16/5/25.
//

import Alamofire
import Foundation
import OSLog

struct EnumeratorPageResponse: Sendable {
    let token: String   // Required by server to serve the next page of items
    let index: Int      // Needed to calculate the offset for the next paginated request
    let total: Int?     // Total item count, provided in the first non-offset paginated response
    let logger = Logger(subsystem: Logger.subsystem, category: "enumeratorpageresponse")

    init?(nkResponseData: AFDataResponse<Data>?, index: Int) {
        guard let headers = nkResponseData?.response?.allHeaderFields as? [String: String] else {
            logger.debug("Page response nil as could not get header fields from Alamofire response")
            return nil
        }

        let normalisedHeaders =
            Dictionary(uniqueKeysWithValues: headers.map { ($0.key.lowercased(), $0.value) })
        guard Bool(normalisedHeaders["x-nc-paginate"]?.lowercased() ?? "false") == true,
              let responsePaginateToken = normalisedHeaders["x-nc-paginate-token"]
        else {
            logger.debug(
                "Page response nil, page headers missing. \(normalisedHeaders, privacy: .public)"
            )
            return nil
        }

        self.index = index
        token = responsePaginateToken
        if let responsePaginateTotal = normalisedHeaders["x-nc-paginate-total"] {
            total = Int(responsePaginateTotal)
        } else {
            total = nil
        }
        let totalString = total != nil ? String(total ?? -1) : "nil"
        logger.debug(
            """
            Created enumerator page response with data from PROPFIND reply.
                token: \(responsePaginateToken, privacy: .public)
                index: \(index, privacy: .public)
                total: \(totalString, privacy: .public)
            """
        )
    }

    init(nextServerUrl: String) {
        logger.debug("Creating artificial page response with \(nextServerUrl, privacy: .public)")
        self.token = nextServerUrl
        self.index = -1
        self.total = nil
    }
}

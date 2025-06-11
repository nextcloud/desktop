//
//  EnumeratorPageResponse.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 16/5/25.
//

import Alamofire
import Foundation
import OSLog

fileprivate let logger = Logger(subsystem: Logger.subsystem, category: "enumeratorpageresponse")

struct EnumeratorPageResponse: Sendable, Codable {
    let token: String?   // Required by server to serve the next page of items
    let index: Int      // Needed to calculate the offset for the next paginated request
    let total: Int?     // Total item count, provided in the first non-offset paginated response
    var serverUrlQueue: [String]?
    var nextServerUrl: String? = nil

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
        nextServerUrl = nil
        serverUrlQueue = nil
        
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

    init(nextServerUrl: String, serverUrlQueue: [String]? = nil) {
        logger.debug("Creating artificial page response with \(nextServerUrl, privacy: .public)")
        self.token = nil
        self.index = 0
        self.total = nil
        self.nextServerUrl = nextServerUrl
        self.serverUrlQueue = serverUrlQueue
    }
}

//
//  NKRequestOptions+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 16/5/25.
//

import FileProvider
import NextcloudKit

extension NKRequestOptions {
    convenience init(page: NSFileProviderPage?, offset: Int? = nil, count: Int? = nil) {
        var token: String? = nil
        if let page {
            token = String(data: page.rawValue, encoding: .utf8)
        }
        self.init(
            paginate: true,
            paginateToken: token,
            paginateOffset: offset,
            paginateCount: count
        )
    }
}

//
//  NKRequestOptions+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 16/5/25.
//

import FileProvider
import NextcloudKit

extension NKRequestOptions {
    convenience init(page: NSFileProviderPage, offset: Int? = nil, count: Int? = nil) {
        self.init(
            paginate: true,
            paginateToken: String(data: page.rawValue, encoding: .utf8),
            paginateOffset: offset,
            paginateCount: count
        )
    }
}

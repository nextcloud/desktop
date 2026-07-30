//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
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

//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

extension NKRequestOptions {
    ///
    /// Request options for one page of a paginated listing.
    ///
    /// The page carries the token the server issued with the previous page, among other things. The server serves a page from its cached result set only for a token it knows; anything else makes it list the whole directory again and issue a new token.
    ///
    convenience init(page: NSFileProviderPage?, offset: Int? = nil, count: Int? = nil) {
        var token: String?
        if let page {
            // The item enumeration hands over the page it encoded, the token among other things; any other page is the token itself.
            token = (try? JSONDecoder().decode(EnumeratorPageResponse.self, from: page.rawValue))?.token
                ?? String(data: page.rawValue, encoding: .utf8)
        }
        self.init(
            paginate: true,
            paginateToken: token,
            paginateOffset: offset,
            paginateCount: count
        )
    }
}

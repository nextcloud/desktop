//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct SafeFilenameUrlTests {
    @Test func safeFilenameFull() {
        let urlString = "https://example.com.cn/something/goes/here.html"
        let expected = "example_com_cn_something_goes_here_html"
        let url = URL(string: urlString)
        #expect(url?.safeFilenameFromURLString() == expected)
    }

    @Test func safeFilenameHostOnly() {
        let urlString = "https://example.com.cn"
        let expected = "example_com_cn"
        let url = URL(string: urlString)
        #expect(url?.safeFilenameFromURLString() == expected)
    }

    @Test func safeFilenameWithQuery() {
        let urlString = "https://www.example.com.cn/path/to/file.html?query=string&param=value&"
        let expected = "www_example_com_cn_path_to_file_html_query=string&param=value&"
        let url = URL(string: urlString)
        #expect(url?.safeFilenameFromURLString() == expected)
    }
}

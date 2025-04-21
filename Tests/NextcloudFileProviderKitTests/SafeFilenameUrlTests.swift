//
//  SafeFilenameUrlTests.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 21/4/25.
//

import Foundation
import Testing
@testable import NextcloudFileProviderKit

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

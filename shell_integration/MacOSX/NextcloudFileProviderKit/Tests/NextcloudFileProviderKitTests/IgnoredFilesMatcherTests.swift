//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

struct IgnoredFilesMatcherTests {
    @Test func patternMatchingWorks() {
        let patterns = [
            "*.tmp",
            "build/",
            "folder/*",
            "secret.txt",
            "deep/**"
        ]

        let matcher = IgnoredFilesMatcher(ignoreList: patterns, log: FileProviderLogMock())

        #expect(matcher.isExcluded("foo.tmp"))
        #expect(matcher.isExcluded("a/b/c/hello.tmp"))
        #expect(matcher.isExcluded("/a/b/c/hello.tmp"))
        #expect(matcher.isExcluded("build/"))
        #expect(!matcher.isExcluded("build")) // We should not match files, just children of build
        #expect(matcher.isExcluded("folder/file.txt"))
        #expect(!matcher.isExcluded("folder/sub/file.txt"))
        #expect(matcher.isExcluded("secret.txt"))
        #expect(!matcher.isExcluded("secret.doc"))
        #expect(matcher.isExcluded("deep/one.txt"))
        #expect(matcher.isExcluded("deep/more/files/here.doc"))
        #expect(!matcher.isExcluded("other/deep/file.txt"))
        #expect(!matcher.isExcluded("random.file"))
    }
}

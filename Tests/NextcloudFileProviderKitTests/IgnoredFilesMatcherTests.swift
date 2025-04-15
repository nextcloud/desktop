//
//  IgnoredFilesMatcherTests.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 15/4/25.
//

import Testing
@testable import NextcloudFileProviderKit

struct IgnoredFilesMatcherTests {
    @Test func patternMatchingWorks() {
        let patterns = [
            "*.tmp",
            "build/",
            "folder/*",
            "secret.txt",
            "deep/**"
        ]

        let matcher = IgnoredFilesMatcher(ignoreList: patterns)

        #expect(matcher.isExcluded("foo.tmp"))
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

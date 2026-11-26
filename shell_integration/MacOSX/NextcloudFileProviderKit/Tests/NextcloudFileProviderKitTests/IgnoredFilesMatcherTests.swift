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
        #expect(matcher.isExcluded("folder/sub/file.txt")) // Below the excluded directory "folder/sub"
        #expect(matcher.isExcluded("secret.txt"))
        #expect(!matcher.isExcluded("secret.doc"))
        #expect(matcher.isExcluded("deep/one.txt"))
        #expect(matcher.isExcluded("deep/more/files/here.doc"))
        #expect(!matcher.isExcluded("other/deep/file.txt"))
        #expect(!matcher.isExcluded("random.file"))
    }

    @Test func excludeAndRemovePrefixMatchesDesktopTemporaryFiles() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["]*.~*"], log: FileProviderLogMock())

        #expect(matcher.isExcluded(".testbestand.docx.~485bd8b"))
        #expect(matcher.isExcluded("folder/.testbestand.docx.~485bd8b"))
        #expect(!matcher.isExcluded("testbestand.docx"))
    }

    // The following cases are ported from `test/testexcludedfiles.cpp`, with paths relative to the root container as the classic sync engine passes them.

    @Test func rootAnchoredPatternsMatchPathsRelativeToTheRootContainer() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["foo/bar", "latex/*/*.tex.tmp", "*/*.out"], log: FileProviderLogMock())

        #expect(matcher.isExcluded("foo/bar"))
        #expect(matcher.isExcluded("foo/bar/baz.txt"))
        #expect(!matcher.isExcluded("other/foo/bar"))
        #expect(!matcher.isExcluded("foo/barbaz"))
        #expect(!matcher.isExcluded("latex/my_manuscript.tex.tmp"))
        #expect(matcher.isExcluded("latex/songbook/my_manuscript.tex.tmp"))
        #expect(matcher.isExcluded("foobar/my_manuscript.out"))
        #expect(!matcher.isExcluded("my_manuscript.out"))
    }

    @Test func patternsWithLeadingSlashNeverMatchAsInTheClassicSyncEngine() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["/exclude"], log: FileProviderLogMock())

        #expect(!matcher.isExcluded("exclude"))
        #expect(!matcher.isExcluded("foo/exclude"))
    }

    @Test func patternsAlsoExcludeEverythingBelowAMatchingDirectory() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["filedir", "dir/", ".apdisk", "*.excl"], log: FileProviderLogMock())

        #expect(matcher.isExcluded("filedir"))
        #expect(matcher.isExcluded("filedir/foo"))
        #expect(!matcher.isExcluded("dir"))
        #expect(matcher.isExcluded("dir/foo"))
        #expect(matcher.isExcluded("s/dir/foo"))
        #expect(matcher.isExcluded("projects/.apdisk/totally_amazing.jar"))
        #expect(matcher.isExcluded("a.excl/b.txt"))
        #expect(!matcher.isExcluded("a.excl.txt"))
    }

    @Test func wildcardsDoNotMatchSlashes() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["a/foo*bar", "c/foo?bar", "g/bar*", "bond00?"], log: FileProviderLogMock())

        #expect(matcher.isExcluded("a/fooXYZbar"))
        #expect(!matcher.isExcluded("a/fooX/Zbar"))
        #expect(matcher.isExcluded("c/fooXbar"))
        #expect(!matcher.isExcluded("c/foo/bar"))
        #expect(matcher.isExcluded("g/barABC"))
        #expect(!matcher.isExcluded("g/XbarABC"))
        #expect(!matcher.isExcluded("bond00"))
        #expect(matcher.isExcluded("bond007"))
        #expect(!matcher.isExcluded("bond0071"))
    }

    @Test func bracketExpressionsMatchCharacterSets() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["a [bc] d", "x[!0-9]y", "unclosed[bracket"], log: FileProviderLogMock())

        #expect(!matcher.isExcluded("a d d"))
        #expect(!matcher.isExcluded("a  d"))
        #expect(matcher.isExcluded("a b d"))
        #expect(matcher.isExcluded("a c d"))
        #expect(matcher.isExcluded("xay"))
        #expect(!matcher.isExcluded("x1y"))
        #expect(matcher.isExcluded("unclosed[bracket"))
    }

    @Test func escapedWildcardsMatchLiterally() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["a \\*", "b \\?", "c \\[d]"], log: FileProviderLogMock())

        #expect(!matcher.isExcluded("a \\*"))
        #expect(!matcher.isExcluded("a bc"))
        #expect(matcher.isExcluded("a *"))
        #expect(!matcher.isExcluded("b \\?"))
        #expect(!matcher.isExcluded("b f"))
        #expect(matcher.isExcluded("b ?"))
        #expect(!matcher.isExcluded("c \\[d]"))
        #expect(!matcher.isExcluded("c d"))
        #expect(matcher.isExcluded("c [d]"))
    }

    @Test func nonAsciiPatternsMatch() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["*.💩", "пятницы.*"], log: FileProviderLogMock())

        #expect(!matcher.isExcluded("unicode/中文.hé"))
        #expect(matcher.isExcluded("unicode/пятницы.txt"))
        #expect(matcher.isExcluded("unicode/ПЯТНИЦЫ.txt"))
        #expect(matcher.isExcluded("unicode/中文.💩"))
    }

    @Test func loneAsteriskIsIgnored() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["*", " * "], log: FileProviderLogMock())

        #expect(!matcher.isExcluded("file.txt"))
        #expect(!matcher.isExcluded("folder/file.txt"))
    }

    @Test func hashIsLiteralInPatterns() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["#hash"], log: FileProviderLogMock())

        #expect(matcher.isExcluded("#hash"))
        #expect(!matcher.isExcluded("hash"))
    }

    @Test func excludeListFileIsOnlyExcludedThroughItsParentDirectories() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["*.lst", ".*", "build"], log: FileProviderLogMock())

        #expect(!matcher.isExcluded(".sync-exclude.lst"))
        #expect(!matcher.isExcluded("folder/.sync-exclude.lst"))
        #expect(matcher.isExcluded("build/.sync-exclude.lst"))
        #expect(matcher.isExcluded(".hidden/.sync-exclude.lst"))
        #expect(matcher.isExcluded("other.lst"))
    }

    @Test func excludeListFileInDirectoryMatchedByDirectoryOnlyPatternIsExcluded() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["build/"], log: FileProviderLogMock())

        #expect(matcher.isExcluded("build/.sync-exclude.lst", isDirectory: false))
    }

    @Test func directoryOnlyPatternsMatchDirectories() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["filedir", "dir/"], log: FileProviderLogMock())

        #expect(!matcher.isExcluded("s/dir", isDirectory: false))
        #expect(!matcher.isExcluded("other", isDirectory: true))
        #expect(matcher.isExcluded("filedir", isDirectory: true))
        #expect(matcher.isExcluded("dir", isDirectory: true))
        #expect(matcher.isExcluded("s/filedir", isDirectory: true))
        #expect(matcher.isExcluded("s/dir", isDirectory: true))
        #expect(matcher.isExcluded("filedir/foo", isDirectory: true))
        #expect(matcher.isExcluded("dir/foo", isDirectory: true))
    }

    @Test func rootAnchoredDirectoryOnlyPatternsMatchDirectoriesAndTheirContents() {
        let matcher = IgnoredFilesMatcher(ignoreList: ["foo/bar/"], log: FileProviderLogMock())

        #expect(matcher.isExcluded("foo/bar", isDirectory: true))
        #expect(!matcher.isExcluded("foo/bar", isDirectory: false))
        #expect(matcher.isExcluded("foo/bar/baz.txt", isDirectory: false))
        #expect(!matcher.isExcluded("other/foo/bar", isDirectory: true))
    }

    @Test func mergedMatcherExcludesWhatEitherExcludes() {
        let global = IgnoredFilesMatcher(ignoreList: ["*.part"], log: FileProviderLogMock())
        let root = IgnoredFilesMatcher(ignoreList: ["*.excl", "cache/"], log: FileProviderLogMock())
        let merged = global.merging(root)

        #expect(merged.isExcluded("a.part"))
        #expect(merged.isExcluded("sub/a.excl"))
        #expect(merged.isExcluded("sub/cache", isDirectory: true))
        #expect(merged.isExcluded("sub/cache/file", isDirectory: false))
        #expect(!merged.isExcluded("a.txt"))
        #expect(!global.isExcluded("a.excl"))
        #expect(!root.isExcluded("a.part"))
    }

    @Test func readingExcludeListSkipsCommentsAndBlankLinesAndTrimsLines() {
        let contents = "# comment\n\n  *.excl  \r\nfoo/\n\t\n#!version < 2.0.0\nkept-after-version-directive\n*\n"

        #expect(IgnoredFilesMatcher.patterns(inExcludeList: contents) == ["*.excl", "foo/", "kept-after-version-directive", "*"])
    }

    @Test func readingExcludeListExpandsEscapes() {
        let contents = "\\#hash\nicon\\r\ntab\\there\nquestion\\?\nkept\\*\\[\\\\\nunknown\\z"

        #expect(IgnoredFilesMatcher.patterns(inExcludeList: contents) == ["#hash", "icon\r", "tab\there", "question?", "kept\\*\\[\\\\", "unknown\\z"])
    }
}

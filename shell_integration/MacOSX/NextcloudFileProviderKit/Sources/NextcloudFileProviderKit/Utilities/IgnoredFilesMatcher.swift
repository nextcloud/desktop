//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// Decides whether a path relative to the root container is excluded from synchronization.
///
/// Patterns follow `ExcludedFiles` in `src/csync/csync_exclude.cpp`, with every pattern applying from the root container.
///
public final class IgnoredFilesMatcher: Sendable {
    ///
    /// The name of an exclude list file, which is synchronized like any other file.
    ///
    static let excludeListFileName = ".sync-exclude.lst"

    private static let excludeListLineWhitespace = CharacterSet(charactersIn: " \t\n\r\u{0B}\u{0C}")

    private let logger: FileProviderLogger
    private let fileRegexes: [NSRegularExpression]
    private let directoryRegexes: [NSRegularExpression]

    ///
    /// Translate the wildcards of a pattern into regular expression syntax, as `ExcludedFiles::convertToRegexpSyntax` does.
    ///
    static func regex(translating pattern: String, wildcardsMatchSlash: Bool) -> String {
        let scalars = Array(pattern.unicodeScalars)
        var regex = ""
        var literal = String.UnicodeScalarView()

        func flushLiteral() {
            regex += NSRegularExpression.escapedPattern(for: String(literal))
            literal.removeAll()
        }

        var index = 0

        while index < scalars.count {
            let scalar = scalars[index]

            switch scalar {
                case "*":
                    flushLiteral()

                    if index + 1 < scalars.count, scalars[index + 1] == "*" {
                        // Unlike `csync_exclude.cpp`, `**` also matches across directories.
                        regex += ".*"
                        index += 1
                    } else {
                        regex += wildcardsMatchSlash ? ".*" : "[^/]*"
                    }
                case "?":
                    flushLiteral()
                    regex += wildcardsMatchSlash ? "." : "[^/]"
                case "[":
                    flushLiteral()
                    var end = index + 1

                    while end < scalars.count, scalars[end] != "]" {
                        if end + 1 < scalars.count, scalars[end] == "\\", scalars[end + 1] == "]" {
                            end += 1
                        }

                        end += 1
                    }

                    guard end < scalars.count else {
                        regex += "\\["
                        break
                    }

                    var bracketExpression = String(String.UnicodeScalarView(scalars[index ... end]))

                    if bracketExpression.hasPrefix("[!") {
                        bracketExpression = "[^" + bracketExpression.dropFirst(2)
                    }

                    regex += bracketExpression
                    index = end
                case "\\":
                    flushLiteral()

                    guard index + 1 < scalars.count else {
                        regex += "\\\\"
                        break
                    }

                    let escaped = scalars[index + 1]

                    switch escaped {
                        case "*", "?", "[", "\\":
                            regex += NSRegularExpression.escapedPattern(for: String(escaped))
                        default:
                            literal.append(scalar)
                            literal.append(escaped)
                    }

                    index += 1
                default:
                    literal.append(scalar)
            }

            index += 1
        }

        flushLiteral()

        return regex
    }

    ///
    /// The regular expressions for files and for directories of one pattern, built as `ExcludedFiles::prepare` does.
    ///
    /// A pattern also excludes everything below a directory it matches. Unlike `csync_exclude.cpp`, this includes files below a directory matched by a directory-only pattern containing a slash, which the classic sync engine never visits.
    ///
    private static func regexes(for pattern: String, wildcardsMatchSlash: Bool) -> (file: String, directory: String)? {
        var pattern = pattern.trimmingCharacters(in: .whitespaces)

        // A lone `*` would exclude everything, so `csync_exclude.cpp` drops it. Comments are removed when reading a list, so a remaining `#` is literal.
        guard !pattern.isEmpty, pattern != "*" else {
            return nil
        }

        let matchesDirectoriesOnly = pattern.hasSuffix("/")

        if matchesDirectoriesOnly {
            pattern.removeLast()
        }

        // A leading `]` asks to also remove excluded items locally, which is not implemented here, so they are only excluded.
        if pattern.hasPrefix("]") {
            pattern.removeFirst()
        }

        guard !pattern.isEmpty else {
            return nil
        }

        // A pattern containing a slash is anchored to the root container, others match any path component.
        let anchor = pattern.contains("/") ? "^" : "(?:^|/)"
        let regex = anchor + "(?:" + Self.regex(translating: pattern, wildcardsMatchSlash: wildcardsMatchSlash) + ")"
        let directoryRegex = regex + "(?:$|/)"

        return (matchesDirectoriesOnly ? regex + "/" : directoryRegex, directoryRegex)
    }

    ///
    /// Expand C-like escape sequences in a line of an exclude list file, as `csync_exclude_expand_escapes` does.
    ///
    /// `\*`, `\[` and `\\` are kept for ``regex(translating:wildcardsMatchSlash:)``.
    ///
    static func expandingEscapes(_ line: String) -> String {
        let scalars = Array(line.unicodeScalars)
        var expanded = String.UnicodeScalarView()
        var index = 0

        while index < scalars.count {
            let scalar = scalars[index]

            guard scalar == "\\", index + 1 < scalars.count else {
                expanded.append(scalar)
                index += 1
                continue
            }

            let escaped = scalars[index + 1]

            switch escaped {
                case "'", "\"", "?", "#":
                    expanded.append(escaped)
                case "a":
                    expanded.append("\u{07}")
                case "b":
                    expanded.append("\u{08}")
                case "f":
                    expanded.append("\u{0C}")
                case "n":
                    expanded.append("\n")
                case "r":
                    expanded.append("\r")
                case "t":
                    expanded.append("\t")
                case "v":
                    expanded.append("\u{0B}")
                default:
                    expanded.append(scalar)
                    expanded.append(escaped)
            }

            index += 2
        }

        return String(expanded)
    }

    ///
    /// The patterns in the contents of an exclude list file, read as `ExcludedFiles::loadExcludeFilePatterns` does.
    ///
    /// A `#!version` directive is treated as a comment, so the line after it is always kept: the extension does not know the desktop client version to compare with.
    ///
    static func patterns(inExcludeList contents: String) -> [String] {
        contents
            .components(separatedBy: "\n")
            .map { $0.trimmingCharacters(in: excludeListLineWhitespace) }
            .filter { !$0.isEmpty && !$0.hasPrefix("#") }
            .map(expandingEscapes)
    }

    public init(ignoreList: [String], wildcardsMatchSlash: Bool = false, log: any FileProviderLogging) {
        logger = FileProviderLogger(category: "IgnoredFilesMatcher", log: log)
        logger.debug("Initializing with ignore list:\n\n\(ignoreList.map { "- \"\($0)\"" }.joined(separator: "\n"))")

        var fileRegexes = [NSRegularExpression]()
        var directoryRegexes = [NSRegularExpression]()

        for pattern in ignoreList {
            guard let regexes = Self.regexes(for: pattern, wildcardsMatchSlash: wildcardsMatchSlash) else {
                continue
            }

            guard let fileRegex = try? NSRegularExpression(pattern: regexes.file, options: [.caseInsensitive]),
                  let directoryRegex = try? NSRegularExpression(pattern: regexes.directory, options: [.caseInsensitive])
            else {
                logger.error("Ignoring exclude pattern which cannot be translated into a regular expression.", [.name: pattern])
                continue
            }

            fileRegexes.append(fileRegex)
            directoryRegexes.append(directoryRegex)
        }

        self.fileRegexes = fileRegexes
        self.directoryRegexes = directoryRegexes
    }

    private init(logger: FileProviderLogger, fileRegexes: [NSRegularExpression], directoryRegexes: [NSRegularExpression]) {
        self.logger = logger
        self.fileRegexes = fileRegexes
        self.directoryRegexes = directoryRegexes
    }

    ///
    /// A matcher which excludes everything excluded by this matcher or by `other`.
    ///
    func merging(_ other: IgnoredFilesMatcher) -> IgnoredFilesMatcher {
        IgnoredFilesMatcher(
            logger: logger,
            fileRegexes: fileRegexes + other.fileRegexes,
            directoryRegexes: directoryRegexes + other.directoryRegexes
        )
    }

    ///
    /// Whether an item is excluded from synchronization.
    ///
    /// - Parameters:
    ///     - relativePath: The path of the item relative to the root container, without a leading slash.
    ///     - isDirectory: Whether the item is a directory, which directory-only patterns ending with a slash depend on.
    ///
    func isExcluded(_ relativePath: String, isDirectory: Bool = false) -> Bool {
        let fileNameStart = relativePath.lastIndex(of: "/").map { relativePath.index(after: $0) } ?? relativePath.startIndex

        // Other clients read exclude list files too, so only an excluded parent directory keeps one from being synchronized.
        if !isDirectory, relativePath[fileNameStart...] == Self.excludeListFileName {
            guard fileNameStart > relativePath.startIndex else {
                return false
            }

            let parentPath = String(relativePath[..<relativePath.index(before: fileNameStart)])
            return Self.anyMatches(directoryRegexes, parentPath)
        }

        return Self.anyMatches(isDirectory ? directoryRegexes : fileRegexes, relativePath)
    }

    private static func anyMatches(_ regexes: [NSRegularExpression], _ path: String) -> Bool {
        let range = NSRange(location: 0, length: path.utf16.count)
        return regexes.contains { $0.firstMatch(in: path, options: [], range: range) != nil }
    }
}

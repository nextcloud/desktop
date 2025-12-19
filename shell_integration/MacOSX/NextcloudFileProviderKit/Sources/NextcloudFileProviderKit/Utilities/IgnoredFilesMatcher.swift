//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

public class IgnoredFilesMatcher {
    private let regexes: [NSRegularExpression]

    private static func patternToRegex(_ pattern: String, wildcardsMatchSlash: Bool) -> String {
        let trimmed = pattern.trimmingCharacters(in: .whitespaces)
        guard !trimmed.isEmpty, !trimmed.hasPrefix("#") else { return "a^" }

        var regex = ""
        var i = trimmed.startIndex

        let hasSlash = trimmed.contains("/")

        while i < trimmed.endIndex {
            let c = trimmed[i]
            switch c {
                case "*":
                    let next = trimmed.index(after: i)
                    if next < trimmed.endIndex, trimmed[next] == "*" {
                        regex.append(wildcardsMatchSlash ? ".*" : ".*")
                        i = trimmed.index(after: next)
                    } else {
                        regex.append(wildcardsMatchSlash ? ".*" : "[^/]*")
                        i = next
                    }
                case "?":
                    regex.append(wildcardsMatchSlash ? "." : "[^/]")
                    i = trimmed.index(after: i)
                case ".", "[", "]", "(", ")", "{", "}", "+", "^", "$", "|", "\\":
                    regex.append("\\\(c)")
                    i = trimmed.index(after: i)
                default:
                    regex.append(c)
                    i = trimmed.index(after: i)
            }
        }

        return hasSlash ? "^\(regex)$" : "(^|/)" + regex + "$"
    }

    public init(ignoreList: [String], wildcardsMatchSlash: Bool = false) {
        regexes = ignoreList
            .map { Self.patternToRegex($0, wildcardsMatchSlash: wildcardsMatchSlash) }
            .compactMap { try? NSRegularExpression(pattern: $0, options: [.caseInsensitive]) }
    }

    func isExcluded(_ relativePath: String) -> Bool {
        for regex in regexes {
            let range = NSRange(location: 0, length: relativePath.utf16.count)
            if regex.firstMatch(in: relativePath, options: [], range: range) != nil {
                return true
            }
        }
        return false
    }
}

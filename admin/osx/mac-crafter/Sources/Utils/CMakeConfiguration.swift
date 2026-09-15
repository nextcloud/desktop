// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Reads signing-related values from the client's CMake configuration.
///
enum CMakeConfiguration {
    static func developmentTeamIdentifier(at file: URL) throws -> String {
        let contents: String

        do {
            contents = try String(contentsOf: file, encoding: .utf8)
        } catch {
            throw MacCrafterError.signing("Unable to read \(file.path): \(error.localizedDescription)")
        }

        let pattern = #"(?m)^[ \t]*set[ \t]*\([ \t]*DEVELOPMENT_TEAM[ \t]+[\"]?([^\"\s\)]+)"#
        guard let expression = try? NSRegularExpression(pattern: pattern),
              let match = expression.firstMatch(
                  in: contents,
                  range: NSRange(contents.startIndex..., in: contents)
              ),
              let teamIdentifierRange = Range(match.range(at: 1), in: contents)
        else {
            throw MacCrafterError.signing("Unable to find DEVELOPMENT_TEAM in \(file.path)")
        }

        return String(contents[teamIdentifierRange])
    }
}

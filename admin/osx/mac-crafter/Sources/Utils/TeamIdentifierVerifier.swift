// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Team identifier verification helpers.
///
enum TeamIdentifierVerifier {
    static func teamIdentifier(from codesignOutput: String) -> String? {
        let prefix = "TeamIdentifier="

        guard let line = codesignOutput.split(whereSeparator: \.isNewline).first(where: { $0.hasPrefix(prefix) }) else {
            return nil
        }

        let teamIdentifier = line.dropFirst(prefix.count).trimmingCharacters(in: .whitespacesAndNewlines)
        return teamIdentifier == "not set" || teamIdentifier.isEmpty ? nil : teamIdentifier
    }

    static func validationError(for components: [(location: String, teamIdentifier: String?)]) -> String? {
        guard let firstComponent = components.first else {
            return "Signing verification failed because no signed code components were found"
        }

        guard let expectedTeamIdentifier = firstComponent.teamIdentifier else {
            return "Signing verification failed because \(firstComponent.location) has no TeamIdentifier"
        }

        for component in components.dropFirst() {
            guard let teamIdentifier = component.teamIdentifier else {
                return "Signing verification failed because \(component.location) has no TeamIdentifier"
            }

            guard teamIdentifier == expectedTeamIdentifier else {
                return "Signing verification failed because \(component.location) has TeamIdentifier \(teamIdentifier), expected \(expectedTeamIdentifier) from \(firstComponent.location)"
            }
        }

        return nil
    }
}

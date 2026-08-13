// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Testing
@testable import mac_crafter

struct TeamIdentifierVerifierTests {
    @Test
    func parsesTeamIdentifier() {
        let output = "Identifier=org.nextcloud.desktopclient\nTeamIdentifier=NKUJUXUJ3B\n"

        #expect(TeamIdentifierVerifier.teamIdentifier(from: output) == "NKUJUXUJ3B")
    }

    @Test
    func treatsAdHocSignatureAsMissingTeamIdentifier() {
        #expect(TeamIdentifierVerifier.teamIdentifier(from: "Signature=adhoc\nTeamIdentifier=not set\n") == nil)
    }

    @Test
    func rejectsMissingTeamIdentifier() {
        let components = [
            (location: "/Nextcloud.app", teamIdentifier: Optional("NKUJUXUJ3B")),
            (location: "/Nextcloud.app/Contents/Frameworks/QtCore.framework", teamIdentifier: Optional<String>.none),
        ]

        #expect(TeamIdentifierVerifier.validationError(for: components)?.contains("has no TeamIdentifier") == true)
    }

    @Test
    func rejectsMismatchedTeamIdentifier() {
        let components = [
            (location: "/Nextcloud.app", teamIdentifier: Optional("NKUJUXUJ3B")),
            (location: "/Nextcloud.app/Contents/Frameworks/QtCore.framework", teamIdentifier: Optional("QTTEAMID")),
        ]

        let error = TeamIdentifierVerifier.validationError(for: components)

        #expect(error?.contains("QTTEAMID") == true)
        #expect(error?.contains("expected NKUJUXUJ3B") == true)
    }

    @Test
    func acceptsMatchingTeamIdentifiers() {
        let components = [
            (location: "/Nextcloud.app", teamIdentifier: Optional("NKUJUXUJ3B")),
            (location: "/Nextcloud.app/Contents/Frameworks/QtCore.framework", teamIdentifier: Optional("NKUJUXUJ3B")),
        ]

        #expect(TeamIdentifierVerifier.validationError(for: components) == nil)
    }
}

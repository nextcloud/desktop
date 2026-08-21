// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import Testing
@testable import mac_crafter

struct TeamIdentifierVerifierTests {
    private func makeCodeBundleFixture() throws -> (root: URL, machOFiles: Set<String>) {
        let fileManager = FileManager.default
        let root = fileManager.temporaryDirectory
            .appendingPathComponent("mac-crafter-tests-\(UUID().uuidString)")

        let machOPaths = [
            root.appendingPathComponent("Contents/MacOS/Nextcloud"),
            root.appendingPathComponent("Contents/Frameworks/QtCore.framework/Versions/A/QtCore"),
            root.appendingPathComponent("Contents/PlugIns/FileProviderExt.appex/Contents/MacOS/FileProviderExt"),
            root.appendingPathComponent("Contents/PlugIns/Helper.xpc/Contents/MacOS/Helper"),
        ]
        let files = machOPaths + [
            root.appendingPathComponent("Contents/Frameworks/libexample.dylib"),
        ]

        for file in files {
            try fileManager.createDirectory(
                at: file.deletingLastPathComponent(),
                withIntermediateDirectories: true
            )
            fileManager.createFile(atPath: file.path, contents: Data())
        }

        return (root, Set(machOPaths.map { $0.resolvingSymlinksInPath().path }))
    }

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

    @Test
    func discoversBundlesDylibsAndMachOExecutables() throws {
        let fixture = try makeCodeBundleFixture()
        defer { try? FileManager.default.removeItem(at: fixture.root) }

        let components = try CodeSignatureVerifier.discoverCodeComponents(
            at: fixture.root,
            isMachO: { fixture.machOFiles.contains($0.resolvingSymlinksInPath().path) }
        )
        let normalize: (URL) -> String = { $0.resolvingSymlinksInPath().path }
        let discoveredPaths = Set(components.map(normalize))
        let expectedPaths = Set([
            fixture.root.path,
            fixture.root.appendingPathComponent("Contents/Frameworks/QtCore.framework").path,
            fixture.root.appendingPathComponent("Contents/Frameworks/libexample.dylib").path,
            fixture.root.appendingPathComponent("Contents/MacOS/Nextcloud").path,
            fixture.root.appendingPathComponent("Contents/PlugIns/FileProviderExt.appex").path,
            fixture.root.appendingPathComponent("Contents/PlugIns/FileProviderExt.appex/Contents/MacOS/FileProviderExt").path,
            fixture.root.appendingPathComponent("Contents/PlugIns/Helper.xpc").path,
            fixture.root.appendingPathComponent("Contents/PlugIns/Helper.xpc/Contents/MacOS/Helper").path,
            fixture.root.appendingPathComponent("Contents/Frameworks/QtCore.framework/Versions/A/QtCore").path,
        ].map { normalize(URL(fileURLWithPath: $0)) })

        #expect(discoveredPaths == expectedPaths)
    }

    @Test
    func rejectsMismatchedDiscoveredComponent() throws {
        let fixture = try makeCodeBundleFixture()
        defer { try? FileManager.default.removeItem(at: fixture.root) }

        do {
            try CodeSignatureVerifier.verify(
                at: fixture.root,
                isMachO: { fixture.machOFiles.contains($0.resolvingSymlinksInPath().path) },
                signatureDetails: { component in
                    component.path.hasSuffix("QtCore.framework")
                        ? "TeamIdentifier=QTTEAMID"
                        : "TeamIdentifier=NKUJUXUJ3B"
                }
            )
            Issue.record("Expected verification to reject a mismatched discovered component")
        } catch let error as MacCrafterError {
            #expect(error.description.contains("QTTEAMID"))
            #expect(error.description.contains("expected NKUJUXUJ3B"))
        }
    }
}

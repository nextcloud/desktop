/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import Foundation

/// Code signing requirement used to authenticate every peer that connects to the broker.
///
/// The broker hands out a capability — the endpoint of the app's listener, which lets the
/// holder drive the whole FinderSync protocol including menu command execution — so it must
/// authenticate both peers before doing so.
///
/// This is expressed as a requirement string and handed to
/// `NSXPCListener.setConnectionCodeSigningRequirement(_:)` (macOS 13+), never evaluated by
/// hand. Doing the check manually means reading the peer's identity from a PID, which is
/// racy: PIDs are recycled, so between reading the PID and inspecting it the peer can exit
/// and a different, legitimately signed binary can occupy the same PID. The system API uses
/// the message's audit token, which embeds a PID generation and cannot be aliased that way.
enum PeerRequirement {
    /// Requirement text permitting only our own app, FinderSync extension and broker,
    /// signed by our team, distributed either through the Mac App Store or with Developer ID.
    ///
    /// The two certificate clauses are deliberately combined with `or` so that one string
    /// stays valid across both distribution channels; see TN3127, which also explains why
    /// requirements should be derived from `codesign --display -r -` output rather than
    /// invented — the field OIDs below are taken from Xcode's own designated requirements.
    ///
    /// - Important: `teamIdentifier` must be the team the bundles are *actually signed with*, not
    ///   merely the one configured at build time. That is already a precondition elsewhere — the
    ///   App Group identifier is `<team>.<reverse domain>`, and an app-group entitlement is only
    ///   honoured when prefixed by the signing team — so if the two diverge the Mach lookup fails
    ///   before this requirement is ever evaluated. A branded build therefore has to set
    ///   DEVELOPMENT_TEAM to its own team regardless of this check; nothing here assumes
    ///   Nextcloud's.
    ///
    /// - Parameters:
    ///   - teamIdentifier: Team ID the peers are signed with.
    ///   - identifiers: Permitted signing identifiers (bundle IDs).
    ///   - allowAppleDevelopment: Whether to additionally accept Apple Development signatures.
    ///     Only ever true in debug builds: the Apple Development designated requirement is
    ///     *not* mutually compatible with the Developer ID and Mac App Store ones, so a
    ///     locally built peer is rejected by the production requirement no matter how it is
    ///     signed.
    static func text(teamIdentifier: String,
                     identifiers: [String],
                     allowAppleDevelopment: Bool) -> String {
        let identifierClause = identifiers
            .map { "identifier \"\($0)\"" }
            .joined(separator: " or ")

        // Mac App Store distribution.
        var certificateClauses = [
            "certificate leaf[field.1.2.840.113635.100.6.1.9]"
        ]

        // Developer ID distribution, pinned to whichever team built this bundle — not to
        // Nextcloud's. `teamIdentifier` comes from NCDevelopmentTeam in the Info.plist, which the
        // build system templates from DEVELOPMENT_TEAM, so a branded build signed by another team
        // pins that team instead.
        certificateClauses.append("""
            (certificate 1[field.1.2.840.113635.100.6.2.6] \
            and certificate leaf[field.1.2.840.113635.100.6.1.13] \
            and certificate leaf[subject.OU] = "\(teamIdentifier)")
            """)

        if allowAppleDevelopment {
            certificateClauses.append("""
                (certificate 1[field.1.2.840.113635.100.6.2.1] \
                and certificate leaf[field.1.2.840.113635.100.6.1.12] \
                and certificate leaf[subject.OU] = "\(teamIdentifier)")
                """)
        }

        return """
            anchor apple generic \
            and (\(identifierClause)) \
            and (\(certificateClauses.joined(separator: " or ")))
            """
    }
}

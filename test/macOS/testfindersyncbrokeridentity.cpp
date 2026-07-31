/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include "config.h"
#include "macOS/findersyncbrokeridentity.h"

using namespace OCC::Mac;

/**
 * @brief Pins the identifiers the FinderSync XPC rendezvous depends on.
 *
 * Three bundles have to agree on the broker's Mach service name, and each derives it a
 * different way: the client from config.h (the code under test), the extension by appending to
 * NCApplicationGroupIdentifier from its Info.plist, and the broker from its own
 * PRODUCT_BUNDLE_IDENTIFIER. A mismatch does not fail to compile or fail to launch — it produces
 * an XPC lookup that never resolves, which is precisely how 34.0.0 shipped with FinderSync
 * completely inert.
 *
 * These tests cover the three *derivations*. The remaining half of the agreement — that the built
 * login item's CFBundleIdentifier really equals its wrapper filename, which is what SMAppService
 * resolves it by — cannot be checked here because it needs a built bundle; it is asserted at
 * signing time by Signer.assertLoginItemIdentifierMatchesFilename in mac-crafter.
 *
 * So these tests assert the shape of the strings rather than trusting the three copies to stay
 * aligned by inspection. They deliberately do not read any bundle: they must pass in CI, which
 * has no signing identity, no GUI session and never launches the app.
 */
class TestFinderSyncBrokerIdentity : public QObject
{
    Q_OBJECT

private slots:
    void testAppGroupIsTeamPrefixed()
    {
        const auto appGroup = FinderSyncBrokerIdentity::appGroupIdentifier();

        // On macOS an app group must begin with the team identifier to be unrestricted, i.e.
        // usable without a provisioning profile and without prompting for container access.
        QVERIFY(appGroup.startsWith(QStringLiteral(DEVELOPMENT_TEAM) + QLatin1Char('.')));
        QCOMPARE(appGroup,
                 QStringLiteral(DEVELOPMENT_TEAM) + QLatin1Char('.') + QStringLiteral(APPLICATION_REV_DOMAIN));

        // No "group." prefix: that is an iOS convention and gets the entitlement rejected here.
        QVERIFY(!appGroup.startsWith(QStringLiteral("group.")));
    }

    void testBrokerServiceNameMatchesTheExtensionsDerivation()
    {
        const auto expected = FinderSyncBrokerIdentity::brokerServiceName();

        // How FinderSyncXPCManager.m's brokerServiceName() builds it, from the app group in the
        // extension's Info.plist. Both sides must land on the same string or the extension looks
        // up a name nobody vends.
        const auto asTheExtensionDerivesIt =
            FinderSyncBrokerIdentity::appGroupIdentifier() + QStringLiteral(".FinderSyncBroker");

        QCOMPARE(expected, asTheExtensionDerivesIt);
    }

    void testBrokerServiceNameSatisfiesLoginItemAndSandboxRules()
    {
        const auto serviceName = FinderSyncBrokerIdentity::brokerServiceName();
        const auto appGroup = FinderSyncBrokerIdentity::appGroupIdentifier();

        // A Service Management login item may only vend a Mach service named after its own
        // bundle identifier, and that identifier must be team-prefixed.
        QVERIFY(serviceName.startsWith(QStringLiteral(DEVELOPMENT_TEAM) + QLatin1Char('.')));

        // The same name has to sit strictly inside the app group prefix, because that is what
        // grants both sandboxed peers mach-lookup without a temporary-exception entitlement.
        QVERIFY(serviceName.startsWith(appGroup + QLatin1Char('.')));
        QVERIFY(serviceName.length() > appGroup.length() + 1);
    }

    void testPeerRequirementIsWellFormedAndPinsOurTeam()
    {
        const auto requirement = FinderSyncBrokerIdentity::peerRequirement();

        QVERIFY(requirement.startsWith(QStringLiteral("anchor apple generic")));

        // Anchoring alone would accept anything Apple-signed, so the team has to be pinned too.
        QVERIFY(requirement.contains(QStringLiteral("certificate leaf[subject.OU] = \"" DEVELOPMENT_TEAM "\"")));

        // All three peers must be accepted, or the broker rejects the very processes it exists
        // to introduce to each other.
        QVERIFY(requirement.contains(QStringLiteral("identifier \"" APPLICATION_REV_DOMAIN "\"")));
        QVERIFY(requirement.contains(QStringLiteral("identifier \"" APPLICATION_REV_DOMAIN ".FinderSyncExt\"")));
        QVERIFY(requirement.contains(
            QStringLiteral("identifier \"%1\"").arg(FinderSyncBrokerIdentity::brokerServiceName())));

        // Balanced parentheses: csreq rejects a malformed requirement, and
        // -setCodeSigningRequirement: raises on one, so a typo here is a launch-time crash.
        int depth = 0;
        for (const auto character : requirement) {
            if (character == QLatin1Char('(')) {
                ++depth;
            } else if (character == QLatin1Char(')')) {
                --depth;
                QVERIFY2(depth >= 0, "unbalanced ')' in the peer requirement");
            }
        }
        QCOMPARE(depth, 0);
    }

    void testPeerRequirementCoversBothDistributionChannels()
    {
        const auto requirement = FinderSyncBrokerIdentity::peerRequirement();

        // One string has to satisfy Developer ID today and the Mac App Store later, so both
        // certificate marker OIDs must be present. See TN3127.
        QVERIFY2(requirement.contains(QStringLiteral("field.1.2.840.113635.100.6.1.9")),
                 "missing the Mac App Store certificate marker");
        QVERIFY2(requirement.contains(QStringLiteral("field.1.2.840.113635.100.6.1.13")),
                 "missing the Developer ID Application certificate marker");
    }

    void testReleaseBuildsRejectAppleDevelopmentSignatures()
    {
        const auto requirement = FinderSyncBrokerIdentity::peerRequirement();
        const auto appleDevelopmentMarker = QStringLiteral("field.1.2.840.113635.100.6.1.12");

#ifdef QT_NO_DEBUG
        // Shipping a build that trusts locally signed peers would let any developer-signed
        // process on the machine drive the FinderSync protocol.
        QVERIFY2(!requirement.contains(appleDevelopmentMarker),
                 "release builds must not accept Apple Development signatures");
#else
        // Debug builds have to accept them, or a locally built client cannot reach its own
        // broker: the Apple Development designated requirement is not compatible with either
        // production clause.
        QVERIFY2(requirement.contains(appleDevelopmentMarker),
                 "debug builds must accept Apple Development signatures");
#endif
    }
};

QTEST_APPLESS_MAIN(TestFinderSyncBrokerIdentity)
#include "testfindersyncbrokeridentity.moc"

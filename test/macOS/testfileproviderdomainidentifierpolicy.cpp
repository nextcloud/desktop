/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include "macOS/fileproviderdomainidentifierpolicy.h"

using namespace OCC::Mac::FileProviderDomainIdentifierPolicy;

class TestFileProviderDomainIdentifierPolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void listingFailureDoesNotMintANewIdentifier()
    {
        const auto stored = QStringLiteral("0bd6be4e-6151-4db4-9668-57d8503d6d3f");
        const auto decision = decideRegistration(stored, {}, false);
        QCOMPARE(decision.action, RegistrationAction::Abort);
    }

    void listingFailureWithNoStoredIdentifierAborts()
    {
        const auto decision = decideRegistration({}, {}, false);
        QCOMPARE(decision.action, RegistrationAction::Abort);
    }

    void storedIdentifierAlreadyRegisteredIsSkipped()
    {
        const auto stored = QStringLiteral("b375bcfe-1653-457b-ab49-fca678c8cd6d");
        const auto decision = decideRegistration(stored, {stored}, true);
        QCOMPARE(decision.action, RegistrationAction::Skip);
    }

    void vanishedStoredIdentifierIsReusedNotReplaced()
    {
        const auto stored = QStringLiteral("0bd6be4e-6151-4db4-9668-57d8503d6d3f");
        const auto other = QStringLiteral("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
        const auto decision = decideRegistration(stored, {other}, true);
        QCOMPARE(decision.action, RegistrationAction::AddStored);
    }

    void emptyListingWithStoredIdentifierReusesStored()
    {
        const auto stored = QStringLiteral("0bd6be4e-6151-4db4-9668-57d8503d6d3f");
        const auto decision = decideRegistration(stored, {}, true);
        QCOMPARE(decision.action, RegistrationAction::AddStored);
    }

    void firstEnableMintsAFreshIdentifier()
    {
        const auto decision = decideRegistration({}, {}, true);
        QCOMPARE(decision.action, RegistrationAction::AddFresh);
    }
};

QTEST_APPLESS_MAIN(TestFileProviderDomainIdentifierPolicy)
#include "testfileproviderdomainidentifierpolicy.moc"

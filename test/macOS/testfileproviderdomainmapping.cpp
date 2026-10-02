/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/fileproviderdomainmapping.h"

#include <QTest>

using namespace OCC::Mac::FileProviderDomainMapping;
using namespace Qt::StringLiterals;

class TestFileProviderDomainMapping : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void existingRegisteredIdentifierIsKept()
    {
        const QList<DomainDescriptor> domains{
            {u"registered"_s, u"alice@example.com"_s, u"VOLUME-A"_s},
            {u"replacement"_s, u"alice@example.com"_s, u"VOLUME-A"_s},
        };

        QVERIFY(domainIdentifierToAdopt(u"alice@example.com"_s, u"VOLUME-A"_s, u"registered"_s, domains).isEmpty());
    }

    void staleIdentifierAdoptsMatchingExternalDomainDeterministically()
    {
        const QList<DomainDescriptor> domains{
            {u"z-domain"_s, u"alice@example.com"_s, u"volume-a"_s},
            {u"a-domain"_s, u"alice@example.com"_s, u"VOLUME-A"_s},
            {u"other-account"_s, u"bob@example.com"_s, u"VOLUME-A"_s},
            {u"other-volume"_s, u"alice@example.com"_s, u"VOLUME-B"_s},
            {u"internal"_s, u"alice@example.com"_s, {}},
        };

        QCOMPARE(domainIdentifierToAdopt(u"alice@example.com"_s, u"VOLUME-A"_s, u"stale"_s, domains), u"a-domain"_s);
    }

    void remountedDisconnectedDomainIsAdoptedAndReconnected()
    {
        const QList<DomainDescriptor> domains{
            {u"external-domain"_s, u"alice@example.com"_s, u"VOLUME-A"_s, true},
        };

        const auto adoptedIdentifier = domainIdentifierToAdopt(u"alice@example.com"_s, u"VOLUME-A"_s, u"stale-domain"_s, domains);

        QCOMPARE(adoptedIdentifier, u"external-domain"_s);
        QCOMPARE(domainIdentifierToReconnect(adoptedIdentifier, true, domains), u"external-domain"_s);
    }

    void connectedDomainOrDisconnectedAccountDoesNotReconnect()
    {
        const QList<DomainDescriptor> connectedDomain{
            {u"external-domain"_s, u"alice@example.com"_s, u"VOLUME-A"_s, false},
        };
        const QList<DomainDescriptor> disconnectedDomain{
            {u"external-domain"_s, u"alice@example.com"_s, u"VOLUME-A"_s, true},
        };

        QVERIFY(domainIdentifierToReconnect(u"external-domain"_s, true, connectedDomain).isEmpty());
        QVERIFY(domainIdentifierToReconnect(u"external-domain"_s, false, disconnectedDomain).isEmpty());
    }

    void missingAccountOrVolumeDoesNotAdoptDomain()
    {
        const QList<DomainDescriptor> domains{
            {u"domain"_s, u"alice@example.com"_s, u"VOLUME-A"_s},
        };

        QVERIFY(domainIdentifierToAdopt({}, u"VOLUME-A"_s, {}, domains).isEmpty());
        QVERIFY(domainIdentifierToAdopt(u"alice@example.com"_s, {}, {}, domains).isEmpty());
    }

    void orphanCleanupLeavesForeignExternalDomainsAlone()
    {
        const QList<DomainDescriptor> domains{
            {u"owned-internal"_s, {}, {}},
            {u"orphan-internal"_s, {}, {}},
            {u"foreign-external"_s, u"alice@example.com"_s, u"VOLUME-A"_s},
        };
        const QSet<QString> configured{u"owned-internal"_s};

        QCOMPARE(removableOrphanDomainIdentifiers(configured, domains), QStringList{u"orphan-internal"_s});
    }
};

QTEST_GUILESS_MAIN(TestFileProviderDomainMapping)
#include "testfileproviderdomainmapping.moc"

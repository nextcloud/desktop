/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/fileproviderexternalaccountregistry.h"

#include <QtTest>

using namespace OCC::Mac::FileProviderExternalAccountRegistry;
using namespace Qt::StringLiterals;

class TestFileProviderExternalAccountRegistry : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testIncludesOnlyExternallyStoredAccounts()
    {
        const QList<QPair<QString, QString>> accounts{
            {u"bob@example.com"_s, u"volume-b"_s},
            {u"alice@example.com"_s, {}},
            {u"carol@example.com"_s, u"volume-c"_s},
        };

        QCOMPARE(configuredAccountIdentifiers(accounts), QStringList({u"bob@example.com"_s, u"carol@example.com"_s}));
    }

    void testSortsAndDeduplicatesIdentifiers()
    {
        const QList<QPair<QString, QString>> accounts{
            {u"z@example.com"_s, u"volume-z"_s},
            {u"a@example.com"_s, u"volume-a"_s},
            {u"z@example.com"_s, u"volume-z2"_s},
        };

        QCOMPARE(configuredAccountIdentifiers(accounts), QStringList({u"a@example.com"_s, u"z@example.com"_s}));
    }
};

QTEST_APPLESS_MAIN(TestFileProviderExternalAccountRegistry)

#include "testfileproviderexternalaccountregistry.moc"

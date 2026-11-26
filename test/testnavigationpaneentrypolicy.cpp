/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "navigationpaneentrypolicy.h"

#include <QtTest>

using namespace OCC;
using namespace Qt::StringLiterals;

class TestNavigationPaneEntryPolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testIsLegacyEntryOfFolder_data()
    {
        QTest::addColumn<QString>("applicationName");
        QTest::addColumn<QString>("targetFolderPath");
        QTest::addColumn<QStringList>("folderPaths");
        QTest::addColumn<bool>("expected");

        const auto sync = uR"(C:\Users\user\sync)"_s;
        const auto work = uR"(D:\Sync\work)"_s;

        QTest::newRow("synced folder") << QString() << sync << QStringList{sync} << true;
        QTest::newRow("one of several folders") << QString() << work << QStringList{sync, work} << true;
        QTest::newRow("separators, trailing separator, case") << QString() << uR"(c:\users\USER\sync\)"_s << QStringList{u"C:/Users/user/sync"_s} << true;
        QTest::newRow("with application name") << u"App"_s << sync << QStringList{sync} << false;
        QTest::newRow("other folder") << QString() << work << QStringList{sync} << false;
        QTest::newRow("folder with suffix") << QString() << sync + u" (3)"_s << QStringList{sync} << false;
        QTest::newRow("no target") << QString() << QString() << QStringList{sync} << false;
        QTest::newRow("no folders") << QString() << sync << QStringList{} << false;
        QTest::newRow("empty folder path") << QString() << u"."_s << QStringList{QString()} << false;
    }

    void testIsLegacyEntryOfFolder()
    {
        QFETCH(QString, applicationName);
        QFETCH(QString, targetFolderPath);
        QFETCH(QStringList, folderPaths);
        QFETCH(bool, expected);

        QCOMPARE(NavigationPaneEntryPolicy::isLegacyEntryOfFolder(applicationName, targetFolderPath, folderPaths), expected);
    }
};

QTEST_GUILESS_MAIN(TestNavigationPaneEntryPolicy)
#include "testnavigationpaneentrypolicy.moc"

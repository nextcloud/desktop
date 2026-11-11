/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <QDir>

#include "common/utility_mac_sandbox.h"

class TestMacSandboxUtility : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void userHomeDirectoryIsOutsideTheAppContainer()
    {
        const auto userHomeDirectory = OCC::Utility::getRealHomeDirectory();

        QVERIFY(!userHomeDirectory.isEmpty());
        QVERIFY(!userHomeDirectory.contains(QStringLiteral("/Library/Containers/")));
        QVERIFY(QDir(userHomeDirectory).isAbsolute());
    }

    // Tests run unsandboxed, where the home location is the real one; inside the sandbox it is the app container.
    void userHomeDirectoryMatchesTheHomeLocationOutsideTheSandbox()
    {
        QCOMPARE(OCC::Utility::getRealHomeDirectory(), QDir::homePath());
    }
};

QTEST_APPLESS_MAIN(TestMacSandboxUtility)

#include "testmacsandboxutility.moc"

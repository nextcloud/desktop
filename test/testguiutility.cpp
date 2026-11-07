/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>

#include "guiutility.h"

class TestGuiUtility : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Context menu entries use title case on macOS and Linux and sentence case on Windows (nextcloud/desktop#10525).
    void testVfsPinActionTextFollowsPlatformCasing()
    {
#if defined(Q_OS_WIN)
        QCOMPARE(OCC::Utility::vfsPinActionText(), QStringLiteral("Make always available locally"));
#else
        QCOMPARE(OCC::Utility::vfsPinActionText(), QStringLiteral("Make Always Available Locally"));
#endif
    }

    void testVfsFreeSpaceActionTextFollowsPlatformCasing()
    {
#if defined(Q_OS_WIN)
        QCOMPARE(OCC::Utility::vfsFreeSpaceActionText(), QStringLiteral("Free up local space"));
#else
        QCOMPARE(OCC::Utility::vfsFreeSpaceActionText(), QStringLiteral("Free Up Local Space"));
#endif
    }
};

QTEST_GUILESS_MAIN(TestGuiUtility)
#include "testguiutility.moc"

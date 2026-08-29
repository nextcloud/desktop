/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "tray/trayactivationpolicy.h"

#include <QTest>

using namespace OCC;

class TestTrayActivationPolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testPrimaryClickOpensPopup()
    {
        QVERIFY(TrayActivationPolicy::opensPrimaryPopup(QSystemTrayIcon::Trigger));
    }

    void testContextClickBehaviorIsPlatformSpecific()
    {
#ifdef Q_OS_MACOS
        QVERIFY(TrayActivationPolicy::opensPrimaryPopup(QSystemTrayIcon::Context));
#else
        QVERIFY(!TrayActivationPolicy::opensPrimaryPopup(QSystemTrayIcon::Context));
#endif
    }

    void testOtherActivationsDoNotOpenPopup()
    {
        QVERIFY(!TrayActivationPolicy::opensPrimaryPopup(QSystemTrayIcon::Unknown));
        QVERIFY(!TrayActivationPolicy::opensPrimaryPopup(QSystemTrayIcon::DoubleClick));
        QVERIFY(!TrayActivationPolicy::opensPrimaryPopup(QSystemTrayIcon::MiddleClick));
    }
};

QTEST_APPLESS_MAIN(TestTrayActivationPolicy)
#include "testtrayactivationpolicy.moc"

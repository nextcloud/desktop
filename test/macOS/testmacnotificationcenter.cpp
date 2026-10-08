/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/systray.h"

#import <UserNotifications/UserNotifications.h>

#include <QtTest>

class TestMacNotificationCenter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void notificationCenterKeepsDelegateAcrossAutoreleasePool()
    {
        __weak id<UNUserNotificationCenterDelegate> delegate = nil;
        @autoreleasepool {
            OCC::setUserNotificationCenterDelegate();
            delegate = UNUserNotificationCenter.currentNotificationCenter.delegate;
        }
        @autoreleasepool {
            QVERIFY(delegate);
            QVERIFY(delegate == UNUserNotificationCenter.currentNotificationCenter.delegate);
            OCC::setUserNotificationCenterDelegate();
            QVERIFY(delegate == UNUserNotificationCenter.currentNotificationCenter.delegate);
        }
    }
};

QTEST_MAIN(TestMacNotificationCenter)

#include "testmacnotificationcenter.moc"

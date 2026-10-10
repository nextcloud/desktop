/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/progressobserver.h"

#include <QtTest>

class TestMacProgressObserver : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void progressObserverKeepsProgressAndRemovesObservations()
    {
        NSProgress *__attribute__((objc_precise_lifetime)) progress = [[NSProgress alloc] initWithParent:nil userInfo:nil];
        ProgressObserver *__attribute__((objc_precise_lifetime)) observer = nil;
        __weak ProgressObserver *trackedObserver = nil;
        __weak NSProgress *trackedProgress = progress;
        __block auto changes = 0;
        @autoreleasepool {
            observer = [[ProgressObserver alloc] initWithProgress:progress];
            trackedObserver = observer;
            observer.progressKVOChangeHandler = ^(NSProgress *) { ++changes; };
            progress = nil;
        }
        @autoreleasepool {
            QVERIFY(trackedProgress);
            observer.progress.totalUnitCount = 10;
            observer.progress.completedUnitCount = 1;
            QCOMPARE(changes, 2);
            progress = observer.progress;
            observer = nil;
        }
        @autoreleasepool {
            QVERIFY(!trackedObserver);
            progress.completedUnitCount = 2;
            QCOMPARE(changes, 2);
            progress = nil;
        }
        @autoreleasepool {
            QVERIFY(!trackedProgress);
        }
    }
};

QTEST_APPLESS_MAIN(TestMacProgressObserver)

#include "testmacprogressobserver.moc"

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/trayaccountpopup/nchoverview.h"

#include <QtTest>

class TestMacTrayHoverView : public QObject
{
    Q_OBJECT

private:
    static constexpr auto trackingOptions = NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways;

    static NSEvent *trackingEvent(NSEventType type)
    {
        return [NSEvent enterExitEventWithType:type
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                   eventNumber:0
                                trackingNumber:0
                                      userData:nullptr];
    }

    static void verifyTrackingArea(NCHoverView *view)
    {
        QCOMPARE(view.trackingAreas.count, 1);
        const auto area = view.trackingAreas.firstObject;
        QVERIFY(NSEqualRects(area.rect, view.bounds));
        QCOMPARE(area.options, trackingOptions);
        QVERIFY(area.owner == view);
        QVERIFY(!area.userInfo);
    }

private Q_SLOTS:
    void initTestCase()
    {
        [NSApplication sharedApplication];
    }

    void initializesTransparentLayer()
    {
        @autoreleasepool {
            const auto view = [[NCHoverView alloc] init];
            QVERIFY(view);
            QVERIFY(view.wantsLayer);
            QVERIFY(!view.translatesAutoresizingMaskIntoConstraints);
            QVERIFY(CGColorEqualToColor(view.layer.backgroundColor, CGColorGetConstantColor(kCGColorClear)));
        }
    }

    void hoverEventsSetAndClearHighlight()
    {
        @autoreleasepool {
            const auto view = [[NCHoverView alloc] init];
            const auto enteredEvent = trackingEvent(NSEventTypeMouseEntered);
            const auto exitedEvent = trackingEvent(NSEventTypeMouseExited);
            QVERIFY(enteredEvent);
            QVERIFY(exitedEvent);
            for (auto iteration = 0; iteration < 3; ++iteration) {
                [view mouseEntered:enteredEvent];
                const auto expectedColor = [NSColor.labelColor colorWithAlphaComponent:0.08].CGColor;
                QVERIFY(CGColorEqualToColor(view.layer.backgroundColor, expectedColor));
                [view mouseExited:exitedEvent];
                QVERIFY(CGColorEqualToColor(view.layer.backgroundColor, CGColorGetConstantColor(kCGColorClear)));
            }
        }
    }

    void trackingAreaCoversBounds_data()
    {
        QTest::addColumn<QRectF>("bounds");
        QTest::newRow("normal") << QRectF(0, 0, 200, 40);
        QTest::newRow("offset") << QRectF(7, 9, 200, 40);
        QTest::newRow("empty") << QRectF();
    }

    void trackingAreaCoversBounds()
    {
        QFETCH(QRectF, bounds);
        NCHoverView *__attribute__((objc_precise_lifetime)) view = nil;
        @autoreleasepool {
            view = [[NCHoverView alloc] init];
            view.bounds = NSMakeRect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
            [view updateTrackingAreas];
        }
        @autoreleasepool {
            verifyTrackingArea(view);
        }
    }

    void repeatedUpdatesReleasePreviousAreas_data()
    {
        QTest::addColumn<int>("updateCount");
        QTest::newRow("single") << 1;
        QTest::newRow("repeated") << 10;
    }

    void repeatedUpdatesReleasePreviousAreas()
    {
        QFETCH(int, updateCount);
        NCHoverView *__attribute__((objc_precise_lifetime)) view = [[NCHoverView alloc] init];
        const auto areas = [NSHashTable<NSTrackingArea *> weakObjectsHashTable];
        for (auto iteration = 0; iteration < updateCount; ++iteration) {
            @autoreleasepool {
                [view updateTrackingAreas];
                [areas addObject:view.trackingAreas.firstObject];
                verifyTrackingArea(view);
            }
            @autoreleasepool {
                QCOMPARE(areas.allObjects.count, 1);
            }
        }
    }

    void resizingReplacesTrackingArea()
    {
        const auto view = [[NCHoverView alloc] init];
        __weak NSTrackingArea *previousArea = nil;
        @autoreleasepool {
            view.bounds = NSMakeRect(0, 0, 200, 40);
            [view updateTrackingAreas];
            previousArea = view.trackingAreas.firstObject;
        }
        @autoreleasepool {
            view.bounds = NSMakeRect(7, 9, 300, 60);
            [view updateTrackingAreas];
            verifyTrackingArea(view);
        }
        @autoreleasepool {
            QVERIFY(!previousArea);
        }
    }

    void removesAllExistingTrackingAreas()
    {
        const auto view = [[NCHoverView alloc] init];
        const auto previousAreas = [NSHashTable<NSTrackingArea *> weakObjectsHashTable];
        @autoreleasepool {
            for (auto index = 0; index < 2; ++index) {
                const auto area = [[NSTrackingArea alloc] initWithRect:NSMakeRect(index, index, 20, 20) options:trackingOptions owner:nil userInfo:nil];
                [view addTrackingArea:area];
                [previousAreas addObject:area];
            }
        }
        @autoreleasepool {
            QCOMPARE(previousAreas.allObjects.count, 2);
            [view updateTrackingAreas];
            verifyTrackingArea(view);
        }
        @autoreleasepool {
            QCOMPARE(previousAreas.allObjects.count, 0);
        }
    }

    void destroyingViewReleasesTrackingArea()
    {
        __weak NCHoverView *trackedView = nil;
        __weak NSTrackingArea *trackedArea = nil;
        @autoreleasepool {
            NCHoverView *__attribute__((objc_precise_lifetime)) view = [[NCHoverView alloc] init];
            trackedView = view;
            [view updateTrackingAreas];
            trackedArea = view.trackingAreas.firstObject;
            QVERIFY(trackedView);
            QVERIFY(trackedArea);
        }
        @autoreleasepool {
            QVERIFY(!trackedView);
            QVERIFY(!trackedArea);
        }
    }
};

QTEST_MAIN(TestMacTrayHoverView)

#include "testmactrayhoverview.moc"

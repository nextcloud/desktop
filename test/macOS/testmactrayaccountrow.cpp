/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/trayaccountpopup/trayaccountpopupmetrics.h"
#include "ncaccountrowdelegatemock.h"
#include "traypopupviewtestutils.h"

#include <QtTest>

namespace TestUtils = OCC::Mac::TrayPopupViewTestUtils;

class TestMacTrayAccountRow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        [NSApplication sharedApplication];
    }

    void initializesHiddenHoverView()
    {
        @autoreleasepool {
            const auto row = [[NCAccountRow alloc] init];
            QVERIFY(row);
            QVERIFY(row.wantsLayer);
            QVERIFY(!row.translatesAutoresizingMaskIntoConstraints);
            QVERIFY(CGColorEqualToColor(row.layer.backgroundColor, CGColorGetConstantColor(kCGColorClear)));
            QCOMPARE(row.subviews.count, 1);
            const auto hover = row.subviews.firstObject;
            QVERIFY(hover.hidden);
            QVERIFY(hover.wantsLayer);
            QCOMPARE(hover.layer.cornerRadius, kHoverRadius);
            QVERIFY(hover.layer.backgroundColor);
            QVERIFY(!row.popupDelegate);
        }
    }

    void laysOutHoverView_data()
    {
        QTest::addColumn<QRectF>("bounds");
        QTest::newRow("normal") << QRectF(0, 0, kPopupWidth, kRowHeight);
        QTest::newRow("offset") << QRectF(7, 9, kPopupWidth, kRowHeight);
    }

    void laysOutHoverView()
    {
        QFETCH(QRectF, bounds);
        @autoreleasepool {
            const auto row = [[NCAccountRow alloc] init];
            row.frame = NSMakeRect(0, 0, bounds.width(), bounds.height());
            row.bounds = NSMakeRect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
            [row layout];
            const auto actual = row.subviews.firstObject.frame;
            const auto expected = NSInsetRect(row.bounds, kHoverMargin, kAccountHoverVerticalMargin);
            QCOMPARE(QRectF(actual.origin.x, actual.origin.y, actual.size.width, actual.size.height),
                     QRectF(expected.origin.x, expected.origin.y, expected.size.width, expected.size.height));
        }
    }

    void highlightTracksHoverAndPersistentState_data()
    {
        QTest::addColumn<bool>("persistent");
        QTest::addColumn<bool>("mouseInside");
        QTest::newRow("inactive") << false << false;
        QTest::newRow("hover") << false << true;
        QTest::newRow("persistent") << true << false;
        QTest::newRow("persistent-hover") << true << true;
    }

    void highlightTracksHoverAndPersistentState()
    {
        QFETCH(bool, persistent);
        QFETCH(bool, mouseInside);
        @autoreleasepool {
            const auto row = [[NCAccountRow alloc] init];
            const auto entered = TestUtils::mouseEvent(NSEventTypeMouseEntered);
            const auto exited = TestUtils::mouseEvent(NSEventTypeMouseExited);
            QVERIFY(entered);
            QVERIFY(exited);
            [row setPersistentHighlight:persistent];
            if (mouseInside) {
                [row mouseEntered:entered];
            }
            QCOMPARE(bool(row.subviews.firstObject.hidden), !(persistent || mouseInside));
            [row setPersistentHighlight:NO];
            QCOMPARE(bool(row.subviews.firstObject.hidden), !mouseInside);
            [row mouseExited:exited];
            QVERIFY(row.subviews.firstObject.hidden);
            [row setPersistentHighlight:YES];
            [row mouseEntered:entered];
            [row mouseExited:exited];
            QVERIFY(!row.subviews.firstObject.hidden);
        }
    }

    void deliversDelegateCallbacks_data()
    {
        QTest::addColumn<int>("index");
        QTest::newRow("first") << 0;
        QTest::newRow("other") << 5;
        QTest::newRow("negative") << -1;
    }

    void deliversDelegateCallbacks()
    {
        QFETCH(int, index);
        @autoreleasepool {
            const auto row = [[NCAccountRow alloc] init];
            const auto delegate = [[NCAccountRowDelegateMock alloc] init];
            row.popupDelegate = delegate;
            row.userIndex = index;
            const auto entered = TestUtils::mouseEvent(NSEventTypeMouseEntered);
            const auto exited = TestUtils::mouseEvent(NSEventTypeMouseExited);
            const auto clicked = TestUtils::mouseEvent(NSEventTypeLeftMouseUp);
            QVERIFY(entered);
            QVERIFY(exited);
            QVERIFY(clicked);
            [row mouseEntered:entered];
            QCOMPARE(delegate.hoverCount, 1);
            QVERIFY(delegate.hoveredRow == row);
            QVERIFY(!row.subviews.firstObject.hidden);
            [row mouseExited:exited];
            QCOMPARE(delegate.hoverCount, 1);
            QVERIFY(row.subviews.firstObject.hidden);
            [row mouseUp:clicked];
            QCOMPARE(delegate.clickCount, 1);
            QCOMPARE(delegate.clickedIndex, index);
        }
    }

    void clearingDelegateStopsCallbacks()
    {
        @autoreleasepool {
            const auto row = [[NCAccountRow alloc] init];
            const auto delegate = [[NCAccountRowDelegateMock alloc] init];
            row.popupDelegate = delegate;
            row.popupDelegate = nil;
            const auto entered = TestUtils::mouseEvent(NSEventTypeMouseEntered);
            const auto clicked = TestUtils::mouseEvent(NSEventTypeLeftMouseUp);
            QVERIFY(entered);
            QVERIFY(clicked);
            [row mouseEntered:entered];
            [row mouseUp:clicked];
            QCOMPARE(delegate.hoverCount, 0);
            QCOMPARE(delegate.clickCount, 0);
            QVERIFY(!row.subviews.firstObject.hidden);
        }
    }

    void delegateIsNotRetainedAndIsClearedOnDestruction()
    {
        const auto row = [[NCAccountRow alloc] init];
        __weak NCAccountRowDelegateMock *trackedDelegate = nil;
        @autoreleasepool {
            NCAccountRowDelegateMock *__attribute__((objc_precise_lifetime)) delegate = [[NCAccountRowDelegateMock alloc] init];
            trackedDelegate = delegate;
            row.popupDelegate = delegate;
        }
        @autoreleasepool {
            QVERIFY(!trackedDelegate);
            __unsafe_unretained id<NCAccountRowDelegate> observedDelegate = row.popupDelegate;
            QVERIFY(!observedDelegate);
            const auto entered = TestUtils::mouseEvent(NSEventTypeMouseEntered);
            const auto clicked = TestUtils::mouseEvent(NSEventTypeLeftMouseUp);
            QVERIFY(entered);
            QVERIFY(clicked);
            [row mouseEntered:entered];
            [row mouseUp:clicked];
        }
    }

    void rowOwnsHoverViewAcrossAutoreleasePool()
    {
        NCAccountRow *__attribute__((objc_precise_lifetime)) row = nil;
        __weak NCAccountRow *trackedRow = nil;
        __weak NSView *hover = nil;
        @autoreleasepool {
            row = [[NCAccountRow alloc] init];
            trackedRow = row;
            hover = row.subviews.firstObject;
        }
        @autoreleasepool {
            QVERIFY(hover);
            QVERIFY(hover.superview == row);
            [row updateTrackingAreas];
            QCOMPARE(row.trackingAreas.count, 1);
        }
        @autoreleasepool {
            row = nil;
        }
        @autoreleasepool {
            QVERIFY(!trackedRow);
            QVERIFY(!hover);
        }
    }
};

QTEST_MAIN(TestMacTrayAccountRow)

#include "testmactrayaccountrow.moc"

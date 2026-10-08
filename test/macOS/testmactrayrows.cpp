/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/trayaccountpopup/ncactionrow.h"
#include "gui/macOS/trayaccountpopup/ncalertboxrow.h"
#include "gui/macOS/trayaccountpopup/ncsectionheaderrow.h"
#include "gui/macOS/trayaccountpopup/ncspacerview.h"
#include "gui/macOS/trayaccountpopup/ncstaticinforow.h"
#include "gui/macOS/trayaccountpopup/trayaccountpopupviewutils.h"
#include "traypopupviewtestutils.h"

#include <QtTest>

namespace ViewTestUtils = OCC::Mac::TrayPopupViewTestUtils;

class TestMacTrayRows : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        [NSApplication sharedApplication];
    }

    void actionRowOwnsCallbacks_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::newRow("enabled") << true;
        QTest::newRow("disabled") << false;
    }

    void actionRowOwnsCallbacks()
    {
        QFETCH(bool, enabled);
        NCActionRow *__attribute__((objc_precise_lifetime)) row = nil;
        __weak NSObject *capturedObject = nil;
        __block auto clicks = 0;
        __block auto hovers = 0;
        @autoreleasepool {
            const auto object = [[NSObject alloc] init];
            capturedObject = object;
            row = [[NCActionRow alloc] initWithTitle:@"Action"
                width:kPopupWidth
                enabled:enabled
                action:^{
                    if (object) {
                        ++clicks;
                    }
                }
                hoverAction:^(NSView *) {
                    if (object) {
                        ++hovers;
                    }
                }];
        }
        @autoreleasepool {
            QVERIFY(capturedObject);
            [row mouseEntered:ViewTestUtils::mouseEvent(NSEventTypeMouseEntered)];
            [row mouseUp:ViewTestUtils::mouseEvent(NSEventTypeLeftMouseUp)];
            QCOMPARE(clicks, enabled ? 1 : 0);
            QCOMPARE(hovers, enabled ? 1 : 0);
            [row mouseExited:ViewTestUtils::mouseEvent(NSEventTypeMouseExited)];
        }
        @autoreleasepool {
            row = nil;
        }
        @autoreleasepool {
            QVERIFY(!capturedObject);
        }
    }

    void alertRowOwnsCallbacksAndButton()
    {
        NCAlertBoxRow *__attribute__((objc_precise_lifetime)) row = nil;
        __weak NSButton *button = nil;
        __weak NSObject *capturedObject = nil;
        __block auto clicks = 0;
        __block auto hovers = 0;
        @autoreleasepool {
            const auto object = [[NSObject alloc] init];
            capturedObject = object;
            row = [[NCAlertBoxRow alloc] initWithTitle:@"Account alert"
                action:^{
                    if (object) {
                        ++clicks;
                    }
                }
                hoverAction:^(NSView *) {
                    if (object) {
                        ++hovers;
                    }
                }];
            button = static_cast<NSButton *>(row.subviews.lastObject);
        }
        @autoreleasepool {
            QVERIFY(button);
            QVERIFY(button.superview == row);
            [button performClick:nil];
            [row mouseUp:ViewTestUtils::mouseEvent(NSEventTypeLeftMouseUp)];
            [row mouseEntered:ViewTestUtils::mouseEvent(NSEventTypeMouseEntered)];
            QCOMPARE(clicks, 2);
            QCOMPARE(hovers, 1);
            [row updateTrackingAreas];
            [row updateTrackingAreas];
            QCOMPARE(row.trackingAreas.count, 1);
        }
        @autoreleasepool {
            row = nil;
        }
        @autoreleasepool {
            QVERIFY(!button);
            QVERIFY(!capturedObject);
        }
    }

    void rowsAcceptMissingCallbacks()
    {
        @autoreleasepool {
            const auto action = [[NCActionRow alloc] initWithTitle:@"Action" action:nil];
            const auto alert = [[NCAlertBoxRow alloc] initWithTitle:@"Alert" action:nil hoverAction:nil];
            for (NSView *row in @[ action, alert ]) {
                [row mouseEntered:ViewTestUtils::mouseEvent(NSEventTypeMouseEntered)];
                [row mouseUp:ViewTestUtils::mouseEvent(NSEventTypeLeftMouseUp)];
            }
            [static_cast<NSButton *>(alert.subviews.lastObject) performClick:nil];
        }
    }

    void sectionHeaderOwnsLabelAndCopiesTitle()
    {
        NCSectionHeaderRow *__attribute__((objc_precise_lifetime)) row = nil;
        __weak NCSectionHeaderRow *trackedRow = nil;
        __weak NSTextField *label = nil;
        @autoreleasepool {
            const auto title = [NSMutableString stringWithString:@"Recent activity"];
            row = [[NCSectionHeaderRow alloc] initWithTitle:title width:kPopupWidth];
            trackedRow = row;
            label = static_cast<NSTextField *>(row.subviews.firstObject);
            [title setString:@"Changed input"];
        }
        @autoreleasepool {
            QVERIFY(label);
            QVERIFY(label.superview == row);
            QVERIFY([label.stringValue isEqualToString:@"Recent activity"]);
        }
        @autoreleasepool {
            row = nil;
        }
        @autoreleasepool {
            QVERIFY(!trackedRow);
            QVERIFY(!label);
        }
    }

    void staticInfoRowOwnsChildrenAndImage_data()
    {
        QTest::addColumn<bool>("withIcon");
        QTest::newRow("without-icon") << false;
        QTest::newRow("with-icon") << true;
    }

    void staticInfoRowOwnsChildrenAndImage()
    {
        QFETCH(bool, withIcon);
        NCStaticInfoRow *__attribute__((objc_precise_lifetime)) row = nil;
        __weak NCStaticInfoRow *trackedRow = nil;
        __weak NSTextField *label = nil;
        __weak NSImageView *iconView = nil;
        __weak NSImage *image = nil;
        @autoreleasepool {
            const auto icon = withIcon ? [[NSImage alloc] initWithSize:NSMakeSize(16, 16)] : nil;
            row = [[NCStaticInfoRow alloc] initWithTitle:@"No recent activity" icon:icon width:kPopupWidth];
            trackedRow = row;
            label = static_cast<NSTextField *>(row.subviews.firstObject);
            if (withIcon) {
                iconView = static_cast<NSImageView *>(row.subviews.lastObject);
                image = icon;
            }
        }
        @autoreleasepool {
            QVERIFY(label);
            QVERIFY(label.superview == row);
            QVERIFY([label.stringValue isEqualToString:@"No recent activity"]);
            if (withIcon) {
                QVERIFY(iconView);
                QVERIFY(iconView.superview == row);
                QVERIFY(image);
                QVERIFY(iconView.image == image);
            }
        }
        @autoreleasepool {
            row = nil;
        }
        @autoreleasepool {
            QVERIFY(!trackedRow);
            QVERIFY(!label);
            QVERIFY(!iconView);
            QVERIFY(!image);
        }
    }

    void stackOwnsSpacerUntilRemoved_data()
    {
        QTest::addColumn<bool>("defaultWidth");
        QTest::newRow("convenience-initializer") << true;
        QTest::newRow("designated-initializer") << false;
    }

    void stackOwnsSpacerUntilRemoved()
    {
        QFETCH(bool, defaultWidth);
        NSStackView *__attribute__((objc_precise_lifetime)) stack = [[NSStackView alloc] init];
        __weak NCSpacerView *trackedSpacer = nil;
        @autoreleasepool {
            const auto spacer =
                defaultWidth ? [[NCSpacerView alloc] initWithHeight:kTopPadding] : [[NCSpacerView alloc] initWithHeight:kTopPadding width:kAppsPopupWidth];
            trackedSpacer = spacer;
            [stack addArrangedSubview:spacer];
        }
        @autoreleasepool {
            QVERIFY(trackedSpacer);
            QVERIFY(trackedSpacer.superview == stack);
            QCOMPARE(stack.arrangedSubviews.count, 1);
            QCOMPARE(trackedSpacer.fittingSize.height, kTopPadding);
            QCOMPARE(trackedSpacer.fittingSize.width, defaultWidth ? kPopupWidth : kAppsPopupWidth);
        }
        @autoreleasepool {
            OCC::Mac::TrayPopupViewUtils::clearStack(stack);
        }
        @autoreleasepool {
            QCOMPARE(stack.arrangedSubviews.count, 0);
            QVERIFY(!trackedSpacer);
        }
    }
};

QTEST_MAIN(TestMacTrayRows)

#include "testmactrayrows.moc"

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/trayaccountpopup/trayaccountpopupmetrics.h"
#include "gui/macOS/trayaccountpopup/trayaccountpopupviewutils.h"
#include "traypopupviewtestutils.h"

#include <QtTest>

namespace ViewUtils = OCC::Mac::TrayPopupViewUtils;
namespace Caller = OCC::Mac::TrayPopupViewTestUtils;

class TestMacTrayPopupViewUtils : public QObject
{
    Q_OBJECT

private:
    static NSPanel *makePanel()
    {
        return [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, kPopupWidth, kRowHeight)
                                          styleMask:NSWindowStyleMaskBorderless
                                            backing:NSBackingStoreBuffered
                                              defer:YES];
    }

private Q_SLOTS:
    void initTestCase()
    {
        [NSApplication sharedApplication];
    }

    void arcCallerPreservesOwnership()
    {
        const auto stack = [[NSStackView alloc] init];
        const auto views = [NSHashTable<NSView *> weakObjectsHashTable];
        @autoreleasepool {
            Caller::addOwnedView(stack, views);
        }
        @autoreleasepool {
            QCOMPARE(views.allObjects.count, 1);
            QCOMPARE(stack.arrangedSubviews.count, 1);
            QVERIFY(stack.arrangedSubviews.firstObject.superview == stack);
        }
        @autoreleasepool {
            ViewUtils::clearStack(stack);
        }
        @autoreleasepool {
            QCOMPARE(views.allObjects.count, 0);
        }
    }

    void arcCallerKeepsItsOwnReference()
    {
        const auto stack = [[NSStackView alloc] init];
        NSView *__attribute__((objc_precise_lifetime)) view = [[NSView alloc] init];
        __weak NSView *trackedView = view;
        @autoreleasepool {
            ViewUtils::addOwnedArrangedSubview(stack, view);
            ViewUtils::clearStack(stack);
        }
        @autoreleasepool {
            QVERIFY(trackedView);
            QVERIFY(!view.superview);
            QCOMPARE(stack.arrangedSubviews.count, 0);
        }
        @autoreleasepool {
            view = nil;
        }
        QVERIFY(!trackedView);
    }

    void nilStackDoesNotKeepViewAlive()
    {
        const auto views = [NSHashTable<NSView *> weakObjectsHashTable];
        @autoreleasepool {
            Caller::addOwnedView(nil, views);
            ViewUtils::clearStack(nil);
        }
        @autoreleasepool {
            QCOMPARE(views.allObjects.count, 0);
        }
    }

    void clearStackReleasesAllRows_data()
    {
        QTest::addColumn<int>("rowCount");
        QTest::newRow("empty") << 0;
        QTest::newRow("single") << 1;
        QTest::newRow("multiple") << 3;
    }

    void clearStackReleasesAllRows()
    {
        QFETCH(int, rowCount);
        const auto stack = [[NSStackView alloc] init];
        const auto views = [NSHashTable<NSView *> weakObjectsHashTable];
        @autoreleasepool {
            for (auto row = 0; row < rowCount; ++row) {
                Caller::addOwnedView(stack, views);
            }
        }
        @autoreleasepool {
            QCOMPARE(stack.arrangedSubviews.count, rowCount);
            QCOMPARE(views.allObjects.count, rowCount);
            ViewUtils::clearStack(stack);
            QCOMPARE(stack.arrangedSubviews.count, 0);
            QCOMPARE(stack.subviews.count, 0);
        }
        @autoreleasepool {
            QCOMPARE(views.allObjects.count, 0);
            ViewUtils::clearStack(stack);
        }
    }

    void stackCanBeRefilledAfterClearing()
    {
        const auto stack = [[NSStackView alloc] init];
        const auto views = [NSHashTable<NSView *> weakObjectsHashTable];
        for (auto iteration = 0; iteration < 3; ++iteration) {
            @autoreleasepool {
                Caller::addOwnedView(stack, views);
                QCOMPARE(stack.arrangedSubviews.count, 1);
                ViewUtils::clearStack(stack);
            }
            @autoreleasepool {
                QCOMPARE(views.allObjects.count, 0);
            }
        }
    }

    void destroyingStackReleasesItsRows()
    {
        NSStackView *__attribute__((objc_precise_lifetime)) stack = [[NSStackView alloc] init];
        const auto views = [NSHashTable<NSView *> weakObjectsHashTable];
        @autoreleasepool {
            Caller::addOwnedView(stack, views);
        }
        @autoreleasepool {
            QCOMPARE(views.allObjects.count, 1);
        }
        @autoreleasepool {
            stack = nil;
        }
        @autoreleasepool {
            QCOMPARE(views.allObjects.count, 0);
        }
    }

    void panelOwnsItsConfiguredHierarchy()
    {
        NSPanel *__attribute__((objc_precise_lifetime)) panel = nil;
        __weak NSStackView *stack = nil;
        __weak NSView *container = nil;
        __weak NSView *effect = nil;
        @autoreleasepool {
            panel = makePanel();
            stack = ViewUtils::configurePopupPanel(panel);
            container = panel.contentView;
            effect = panel.contentView.subviews.firstObject;
        }
        @autoreleasepool {
            QVERIFY(stack);
            QVERIFY(container);
            QVERIFY(effect);
            QCOMPARE(panel.level, NSPopUpMenuWindowLevel);
            QVERIFY(panel.hasShadow);
            QVERIFY(!panel.releasedWhenClosed);
            QVERIFY(!panel.opaque);
            QVERIFY([panel.backgroundColor isEqual:NSColor.clearColor]);
            QCOMPARE(stack.orientation, NSUserInterfaceLayoutOrientationVertical);
            QCOMPARE(stack.spacing, 0.0);
            QVERIFY(!stack.translatesAutoresizingMaskIntoConstraints);
            QVERIFY(container.wantsLayer);
            QCOMPARE(container.layer.cornerRadius, kCornerRadius);
            QVERIFY(container.layer.masksToBounds);
            QVERIFY(!effect.translatesAutoresizingMaskIntoConstraints);
            QCOMPARE(container.constraints.count, 4);
            if (@available(macOS 26.0, *)) {
                QVERIFY([effect isKindOfClass:NSGlassEffectView.class]);
                QVERIFY(((NSGlassEffectView *)effect).contentView == stack);
                QCOMPARE(((NSGlassEffectView *)effect).cornerRadius, kCornerRadius);
            } else {
                QVERIFY([effect isKindOfClass:NSVisualEffectView.class]);
                QVERIFY(stack.superview == effect);
                QCOMPARE(((NSVisualEffectView *)effect).material, NSVisualEffectMaterialMenu);
                QCOMPARE(effect.constraints.count, 4);
            }
        }
        @autoreleasepool {
            panel = nil;
        }
        @autoreleasepool {
            QVERIFY(!stack);
            QVERIFY(!container);
            QVERIFY(!effect);
        }
    }

    void reconfiguringPanelReleasesPreviousHierarchy()
    {
        NSPanel *__attribute__((objc_precise_lifetime)) panel = nil;
        __weak NSStackView *previousStack = nil;
        __weak NSView *previousContainer = nil;
        @autoreleasepool {
            panel = makePanel();
            previousStack = ViewUtils::configurePopupPanel(panel);
            previousContainer = panel.contentView;
        }
        @autoreleasepool {
            const auto replacement = ViewUtils::configurePopupPanel(panel);
            QVERIFY(replacement);
            QCOMPARE(panel.contentView.subviews.count, 1);
        }
        @autoreleasepool {
            QVERIFY(!previousStack);
            QVERIFY(!previousContainer);
        }
    }
};

QTEST_MAIN(TestMacTrayPopupViewUtils)

#include "testmactraypopupviewutils.moc"

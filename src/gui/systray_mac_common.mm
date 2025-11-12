/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QWindow>
#include <QGuiApplication>
#include <QScreen>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>

#include <cmath>

#import <Cocoa/Cocoa.h>

#include "systray.h"

Q_LOGGING_CATEGORY(lcMacSystrayCommon, "nextcloud.gui.macsystraycommon")

namespace {

CGRect qtGeometryToCocoaRect(const QRect &geometry)
{
    if (!geometry.isValid()) {
        return CGRectNull;
    }

    QScreen *screen = QGuiApplication::screenAt(geometry.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    if (!screen) {
        return CGRectNull;
    }

    const QRect screenGeometry = screen->geometry();

    const CGFloat originX = geometry.x();
    const CGFloat originY = screenGeometry.y() + screenGeometry.height() - geometry.y() - geometry.height();

    return CGRectMake(originX, originY, geometry.width(), geometry.height());
}

NSStatusItem *statusItemForTrayIcon(QSystemTrayIcon *trayIcon)
{
    if (!trayIcon) {
        return nil;
    }

    const CGRect desiredFrame = qtGeometryToCocoaRect(trayIcon->geometry());
    if (CGRectIsNull(desiredFrame)) {
        return nil;
    }

    NSArray *statusItems = [[NSStatusBar systemStatusBar] valueForKey:@"_statusItems"];
    if (![statusItems isKindOfClass:[NSArray class]]) {
        return nil;
    }

    for (NSStatusItem *statusItem in statusItems) {
        NSStatusBarButton *button = statusItem.button;
        if (!button) {
            continue;
        }

        NSWindow *window = button.window;
        if (!window) {
            continue;
        }

        const CGRect windowFrame = NSRectToCGRect(window.frame);

        if (std::abs(windowFrame.origin.x - desiredFrame.origin.x) <= 1.0 &&
            std::abs(windowFrame.origin.y - desiredFrame.origin.y) <= 1.0) {
            return statusItem;
        }
    }

    return nil;
}

} // anonymous namespace

namespace OCC {

double menuBarThickness()
{
    NSMenu * const mainMenu = [[NSApplication sharedApplication] mainMenu];

    if (mainMenu == nil) {
        // Return this educated guess if something goes wrong.
        // As of macOS 12.4 this will always return 22, even on notched Macbooks.
        qCWarning(lcMacSystrayCommon) << "Got nil for main menu. "
                                      << "Going with reasonable menu bar height guess.";
        return NSStatusBar.systemStatusBar.thickness;
    }

    return mainMenu.menuBarHeight;
}

void setTrayWindowLevelAndVisibleOnAllSpaces(QWindow *const window)
{
    NSView * const nativeView = (NSView *)window->winId();
    NSWindow * const nativeWindow = (NSWindow *)(nativeView.window);
    [nativeWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorIgnoresCycle |
                  NSWindowCollectionBehaviorTransient];
    [nativeWindow setLevel:NSMainMenuWindowLevel];
}

bool osXInDarkMode()
{
    NSString * const osxMode = [NSUserDefaults.standardUserDefaults stringForKey:@"AppleInterfaceStyle"];
    return [osxMode containsString:@"Dark"];
}

void setStatusItemToolTip(QSystemTrayIcon *trayIcon, const QString &toolTip)
{
    static bool warnedOnce = false;

    NSStatusItem * const statusItem = statusItemForTrayIcon(trayIcon);
    if (!statusItem) {
        if (!warnedOnce) {
            warnedOnce = true;
            qCWarning(lcMacSystrayCommon) << "Unable to find NSStatusItem for tray icon, tooltip update skipped.";
        }
        return;
    }

    statusItem.button.toolTip = toolTip.isEmpty() ? nil : toolTip.toNSString();
}

}

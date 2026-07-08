/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "nctraypopup.h"

#import "trayaccountpopupmetrics.h"
#import "trayaccountpopupviewutils.h"

#include "systray.h"

#include <QGuiApplication>
#include <QRect>
#include <QScreen>

using namespace OCC::Mac::TrayPopupViewUtils;

static NSScreen *nsScreenForQtScreen(QScreen *qtScreen)
{
    if (!qtScreen) {
        return NSScreen.mainScreen ?: NSScreen.screens.firstObject;
    }

    const auto qtScreenName = qtScreen->name().toNSString();
    for (NSScreen *candidate in NSScreen.screens) {
        if ([candidate.localizedName isEqualToString:qtScreenName]) {
            return candidate;
        }
    }

    const auto qtScreens = QGuiApplication::screens();
    const auto screenIndex = qtScreens.indexOf(qtScreen);
    if (screenIndex >= 0 && screenIndex < static_cast<int>(NSScreen.screens.count)) {
        return [NSScreen.screens objectAtIndex:screenIndex];
    }

    return NSScreen.mainScreen ?: NSScreen.screens.firstObject;
}

namespace OCC {

static NCTrayPopup *s_popup = nil;

void showMacOSTrayPopup(const QRect &iconRect)
{
    if (!s_popup) {
        s_popup = [[NCTrayPopup alloc] init];
    }

    [s_popup populate];

    auto qtScreen = iconRect.isValid() && !iconRect.isNull()
        ? QGuiApplication::screenAt(iconRect.center())
        : nullptr;
    NSScreen *screen = nsScreenForQtScreen(qtScreen);
    if (!screen) {
        screen = NSScreen.screens.firstObject;
    }

    const CGFloat popupW  = s_popup.frame.size.width;
    const CGFloat popupH  = s_popup.frame.size.height;
    const NSRect visibleFrame = screen.visibleFrame;

    CGFloat x, y;
    if (iconRect.isValid() && !iconRect.isNull() && qtScreen) {
        const auto qtScreenGeometry = qtScreen->geometry();
        x = NSMinX(screen.frame) + iconRect.x() - qtScreenGeometry.x() - kStatusItemLeadingOffset;
        y = NSMaxY(screen.frame) - (iconRect.y() + iconRect.height() - qtScreenGeometry.y()) - popupH;
    } else {
        x = NSMaxX(visibleFrame) - popupW - kScreenEdgePadding;
        y = NSMaxY(visibleFrame) - popupH;
    }

    const auto popupOrigin = clampedPopupOrigin(NSMakePoint(x, y), NSMakeSize(popupW, popupH), visibleFrame);

    [s_popup setFrameOrigin:popupOrigin];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
    [s_popup makeKeyAndOrderFront:nil];
}

void hideMacOSTrayPopup()
{
    [s_popup orderOut:nil];
}

} // namespace OCC

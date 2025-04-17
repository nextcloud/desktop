/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QWindow>

#import <Cocoa/Cocoa.h>

#include "systray.h"

Q_LOGGING_CATEGORY(lcMacSystrayCommon, "nextcloud.gui.macsystraycommon")

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

}

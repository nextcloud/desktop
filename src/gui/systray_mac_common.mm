/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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

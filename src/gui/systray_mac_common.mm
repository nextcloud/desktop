/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QGuiApplication>

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

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

void setTrayIconToolTip(QSystemTrayIcon *trayIcon, const QString &toolTip)
{
    // Fix for macOS 15+ (Sequoia) where QSystemTrayIcon::setToolTip() doesn't work properly
    // The root cause is that Apple changed how NSStatusItem tooltips work in macOS 15
    // Tooltips must now be set on NSStatusItem.button instead of the NSStatusItem itself
    
    // Always call the base Qt implementation
    trayIcon->QSystemTrayIcon::setToolTip(toolTip);
    
    // Additional workaround for macOS 15+
    if (@available(macOS 15.0, *)) {
        // Try to access the native NSStatusItem through Qt's internals
        // Qt stores the platform-specific implementation in a private object
        bool tooltipSet = false;
        
        // Search through the tray icon's children to find the native implementation
        const QObjectList children = trayIcon->children();
        for (QObject *child : children) {
            // Qt's platform plugin creates a QCocoaSystemTrayIcon object
            const char *className = child->metaObject()->className();
            if (strstr(className, "SystemTrayIcon") != nullptr) {
                // Try to access the m_statusItem member using Objective-C runtime
                // This is fragile but necessary for the workaround
                unsigned int ivarCount = 0;
                Ivar *ivars = class_copyIvarList(object_getClass((__bridge void *)child), &ivarCount);
                
                for (unsigned int i = 0; i < ivarCount; i++) {
                    const char *ivarName = ivar_getName(ivars[i]);
                    if (strstr(ivarName, "statusItem") != nullptr || strstr(ivarName, "m_statusItem") != nullptr) {
                        // Found the status item member variable
                        NSStatusItem *statusItem = object_getIvar((__bridge id)child, ivars[i]);
                        if (statusItem && statusItem.button) {
                            // Set the tooltip on the button (required for macOS 15+)
                            statusItem.button.toolTip = toolTip.toNSString();
                            tooltipSet = true;
                            qCDebug(lcMacSystrayCommon) << "Successfully set tooltip on NSStatusItem.button for macOS 15+";
                            break;
                        }
                    }
                }
                
                if (ivars) {
                    free(ivars);
                }
                
                if (tooltipSet) {
                    break;
                }
            }
        }
        
        if (!tooltipSet) {
            qCDebug(lcMacSystrayCommon) << "Could not access NSStatusItem directly, tooltip may not work on macOS 15+";
        }
    }
}

}

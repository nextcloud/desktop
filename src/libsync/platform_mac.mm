/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 * Copyright (C) by Erik Verbruggen <erik@verbruggen.consulting>
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

#include "platform_mac.h"

#include <QApplication>
#include <QLoggingCategory>

#import <AppKit/NSApplication.h>

// defined in platform_mac_deprecated.mm
namespace OCC {

void migrateLaunchOnStartup();

}

@interface OwnAppDelegate : NSObject <NSApplicationDelegate>
- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag;
@end

@implementation OwnAppDelegate {
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    if (auto *app = QApplication::instance()) {
        QMetaObject::invokeMethod(app, "showSettingsWindow", Qt::QueuedConnection);
    } else {
        qDebug() << "Failed to call showSettingsWindow slot";
    }
    return YES;
}

@end

namespace OCC {

class MacPlatformPrivate
{
public:
    QMacAutoReleasePool autoReleasePool;
    OwnAppDelegate *appDelegate;
};

MacPlatform::MacPlatform()
    : d_ptr(new MacPlatformPrivate)
{
    Q_D(MacPlatform);

    NSApplicationLoad();
    d->appDelegate = [[OwnAppDelegate alloc] init];
    [[NSApplication sharedApplication] setDelegate:d->appDelegate];

    signal(SIGPIPE, SIG_IGN);
}

MacPlatform::~MacPlatform()
{
    Q_D(MacPlatform);
    [d->appDelegate release];
}

void MacPlatform::migrate()
{
    Platform::migrate();

    migrateLaunchOnStartup();
}

} // namespace OCC

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

#include "application.h"
#include "platform.h"

#import <AppKit/NSApplication.h>

#include <QProcess>

@interface OwnAppDelegate : NSObject <NSApplicationDelegate>
- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag;
@end

@implementation OwnAppDelegate {
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    if (auto app = qobject_cast<OCC::Application *>(QApplication::instance()))
        app->showSettingsDialog();
    return YES;
}

@end

namespace OCC {

// implemented in platform_mac_deprecated.mm
void migrateLaunchOnStartup();

class MacPlatform : public Platform
{
public:
    MacPlatform();
    ~MacPlatform() override;

    void migrate() override;

private:
    QMacAutoReleasePool _autoReleasePool;
    OwnAppDelegate *_appDelegate;
};

MacPlatform::MacPlatform()
{
    NSApplicationLoad();
    _appDelegate = [[OwnAppDelegate alloc] init];
    [[NSApplication sharedApplication] setDelegate:_appDelegate];

    signal(SIGPIPE, SIG_IGN);
}

MacPlatform::~MacPlatform()
{
    [_appDelegate release];
}

void MacPlatform::migrate()
{
    migrateLaunchOnStartup();
}

std::unique_ptr<Platform> Platform::create()
{
    return std::make_unique<MacPlatform>();
}

} // namespace OCC

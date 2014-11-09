/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */


#include <Cocoa/Cocoa.h>
#include <Sparkle/Sparkle.h>
#include <Sparkle/SUUpdater.h>
#include <AppKit/NSApplication.h>

#include "updater/sparkleupdater.h"

#include "utility.h"

#include <QDebug>

// Does not work yet
@interface DelegateObject : NSObject
- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle;
@end
@implementation DelegateObject //(SUUpdaterDelegateInformalProtocol)

// Only possible in later versions, we're not up to date here.
- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle
{
    qDebug() << Q_FUNC_INFO << "may check: YES";
    return YES;
}

// Sent when a valid update is found by the update driver.
- (void)updater:(SUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)update
{
    qDebug() << Q_FUNC_INFO;
}

// Sent when a valid update is not found.
- (void)updaterDidNotFindUpdate:(SUUpdater *)update
{
    qDebug() << Q_FUNC_INFO;
}

// Sent immediately before installing the specified update.
- (void)updater:(SUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    qDebug() << Q_FUNC_INFO;
}

@end


namespace OCC {

class SparkleUpdater::Private
{
    public:
        SUUpdater* updater;
        DelegateObject *delegate;
};

// Delete ~/Library//Preferences/com.owncloud.desktopclient.plist to re-test
SparkleUpdater::SparkleUpdater(const QString& appCastUrl)
    : Updater()
{
    d = new Private;

    d->delegate = [[DelegateObject alloc] init];
    [d->delegate retain];

    d->updater = [SUUpdater sharedUpdater];
    [d->updater setDelegate:d->delegate];
    [d->updater setAutomaticallyChecksForUpdates:YES];
    [d->updater setAutomaticallyDownloadsUpdates:NO];
    [d->updater setSendsSystemProfile:NO];
    [d->updater resetUpdateCycle];
    [d->updater retain];

    NSURL* url = [NSURL URLWithString:
            [NSString stringWithUTF8String: appCastUrl.toUtf8().data()]];
    [d->updater setFeedURL: url];

    // Sparkle 1.8 required
    NSString *userAgent = [NSString stringWithUTF8String: Utility::userAgentString().data()];
    [d->updater setUserAgentString: userAgent];
}

SparkleUpdater::~SparkleUpdater()
{
    [d->updater release];
    delete d;
}

void SparkleUpdater::checkForUpdate()
{
    [d->updater checkForUpdates: NSApp];
}

void SparkleUpdater::backgroundCheckForUpdate()
{
    qDebug() << Q_FUNC_INFO << "launching background check";
    [d->updater checkForUpdatesInBackground];
}

} // namespace OCC

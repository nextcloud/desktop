/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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


#include <Cocoa/Cocoa.h>
#include <Sparkle/Sparkle.h>
#include <Sparkle/SUUpdater.h>
#include <AppKit/NSApplication.h>

#include "common/utility.h"
#include "updater/sparkleupdater.h"

@interface DelegateObject : NSObject <SUUpdaterDelegate>
- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle;
@end
@implementation DelegateObject //(SUUpdaterDelegateInformalProtocol)

- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle
{
    Q_UNUSED(bundle)
    qCDebug(OCC::lcUpdater) << "may check: YES";
    return YES;
}

// Sent when a valid update is found by the update driver.
- (void)updater:(SUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)update
{
    Q_UNUSED(updater)
    Q_UNUSED(update)
    qCDebug(OCC::lcUpdater) << "";
}

// Sent when a valid update is not found.
- (void)updaterDidNotFindUpdate:(SUUpdater *)update
{
    Q_UNUSED(update)
    qCDebug(OCC::lcUpdater) << "";
}

// Sent immediately before installing the specified update.
- (void)updater:(SUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    Q_UNUSED(updater)
    Q_UNUSED(update)
    qCDebug(OCC::lcUpdater) << "";
}

- (void) updater:(SUUpdater *)updater didAbortWithError:(NSError *)error
{
    Q_UNUSED(updater)

    // Warning for end-user:
    qCWarning(OCC::lcUpdater) << error.localizedDescription;

    // Full error, log it for when we need to debug problems (this will hold the full error object,
    // including possible nested `NSError`s):
    qCInfo(OCC::lcUpdater) << "Full error:" << error.description;
}

- (void)updater:(SUUpdater *)updater didFinishLoadingAppcast:(SUAppcast *)appcast
{
    Q_UNUSED(updater)
    Q_UNUSED(appcast)
    qCDebug(OCC::lcUpdater) << "";
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
SparkleUpdater::SparkleUpdater(const QUrl& appCastUrl)
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

    setUpdateUrl(appCastUrl);

    // Sparkle 1.8 required
    NSString *userAgent = [NSString stringWithUTF8String: Utility::userAgentString().data()];
    [d->updater setUserAgentString: userAgent];
}

SparkleUpdater::~SparkleUpdater()
{
    [d->updater release];
    delete d;
}

void SparkleUpdater::setUpdateUrl(const QUrl &url)
{
    NSURL* nsurl = [NSURL URLWithString:
            [NSString stringWithUTF8String: url.toString().toUtf8().data()]];
    [d->updater setFeedURL: nsurl];
}

// FIXME: Should be changed to not instanicate the SparkleUpdater at all in this case
bool autoUpdaterAllowed()
{
    // See https://github.com/owncloud/client/issues/2931
    NSString *bundlePath = [[NSBundle mainBundle] bundlePath];
    NSString *expectedPath = [NSString stringWithFormat:@"/Applications/%@", [bundlePath lastPathComponent]];
    if ([expectedPath isEqualTo:bundlePath]) {
        return true;
    }
    qCWarning(lcUpdater) << "We are not in /Applications, won't check for update!";
    return false;
}


void SparkleUpdater::checkForUpdate()
{
    if (autoUpdaterAllowed()) {
        [d->updater checkForUpdates: NSApp];
    }
}

void SparkleUpdater::backgroundCheckForUpdate()
{
    qCDebug(OCC::lcUpdater) << "launching background check";
    if (autoUpdaterAllowed()) {
        [d->updater checkForUpdatesInBackground];
    }
}

QString SparkleUpdater::statusString()
{
    // FIXME Show the real state depending on the callbacks
    return QString();
}

} // namespace OCC

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
#include <AppKit/NSApplication.h>

#include "updater/sparkleupdater.h"

#include "mirall/utility.h"

namespace Mirall {

class SparkleUpdater::Private
{
    public:
        SUUpdater* updater;
};

SparkleUpdater::SparkleUpdater(const QString& appCastUrl)
    : Updater()
{
    d = new Private;

    d->updater = [SUUpdater sharedUpdater];
    [d->updater retain];

    NSURL* url = [NSURL URLWithString:
            [NSString stringWithUTF8String: appCastUrl.toUtf8().data()]];
    [d->updater setFeedURL: url];

// requires a more recent version
//    NSString *userAgent = [NSString stringWithUTF8String: Utility::userAgentString().data()];
//    [d->updater setUserAgentString: userAgent];
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
    [d->updater checkForUpdatesInBackground];
}

} // namespace Mirall

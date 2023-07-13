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
#include "configfile.h"
#include "updater/sparkleupdater.h"

@class NCSparkleUpdaterDelegate;

class Q_DECL_HIDDEN OCC::SparkleUpdater::SparkleInterface
{
public:
    explicit SparkleInterface(SparkleUpdater *parent)
        : q(parent)
    {
    }

    ~SparkleInterface()
    {
        [updater release];
        [delegate release];
    }

    void statusChanged(const OCC::SparkleUpdater::State state, const QString &statusString = {})
    {
        q->_state = state;
        q->_statusString = statusString;
        emit q->statusChanged();
    }

    SUUpdater* updater;
    NCSparkleUpdaterDelegate *delegate;

private:
    SparkleUpdater * const q;
};


@interface NCSparkleUpdaterDelegate : NSObject <SUUpdaterDelegate>

@property (readwrite, assign) OCC::SparkleUpdater::SparkleInterface *owner;

- (instancetype)initWithOwner:(OCC::SparkleUpdater::SparkleInterface *)owner;
- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle;

@end

@implementation NCSparkleUpdaterDelegate

- (instancetype)initWithOwner:(OCC::SparkleUpdater::SparkleInterface *)owner
{
    self = [super init];
    if (self) {
        _owner = owner;
    }
    return self;
}

- (BOOL)backgroundUpdateChecksAllowed
{
    BOOL allowUpdateCheck = OCC::ConfigFile().skipUpdateCheck() ? NO : YES;
    qCDebug(OCC::lcUpdater) << "Updater may check for updates:" << (allowUpdateCheck ? "YES" : "NO");
    return allowUpdateCheck;
}

- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle
{
    Q_UNUSED(bundle)
    return [self backgroundUpdateChecksAllowed];
}

- (BOOL)updaterShouldShowUpdateAlertForScheduledUpdate:(SUUpdater *)updater forItem:(SUAppcastItem *)item
{
    Q_UNUSED(updater)
    Q_UNUSED(item)
    return [self backgroundUpdateChecksAllowed];
}

- (void)notifyStateChange:(const OCC::SparkleUpdater::State)state displayStatus:(const QString&)statusString
{
    qCDebug(OCC::lcUpdater) << statusString;
    _owner->statusChanged(state, statusString);
}

// Sent when a valid update is found by the update driver.
- (void)updater:(SUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)update
{
    Q_UNUSED(updater)
    Q_UNUSED(update)

    const auto versionQstring = QString::fromNSString(update.displayVersionString);
    const auto message = QObject::tr("Found a valid update: version %1", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::AwaitingUserInput
              displayStatus:message];
}

// Sent when a valid update is not found.
- (void)updaterDidNotFindUpdate:(SUUpdater *)updater
{
    Q_UNUSED(updater)
    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:QObject::tr("No valid update found.")];
}

// Sent immediately before installing the specified update.
- (void)updater:(SUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(update.displayVersionString);
    const auto message = QObject::tr("About to install version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(SUUpdater *)updater didAbortWithError:(NSError *)error
{
    Q_UNUSED(updater)

    const auto errorQstring = QString::fromNSString(error.localizedDescription);
    const auto message = QObject::tr("Aborted with error: %1", "%1 is version number").arg(errorQstring);
    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(SUUpdater *)updater didFinishLoadingAppcast:(SUAppcast *)appcast
{
    Q_UNUSED(updater)
    Q_UNUSED(appcast)
    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:QObject::tr("Finished loading appcast.")];
}

- (void)updater:(SUUpdater *)updater didDismissUpdateAlertPermanently:(BOOL)permanently forItem:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto permanencyString = permanently ? QObject::tr("Permanently") : QObject::tr("Temporarily");
    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("%1 dismissed %2 update", "%1 is permanently or temporarily, %2 is version number").arg(permanencyString, versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater userDidSkipThisVersion:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Update version %1 will not be applied as it was chosen to be skipped.",  "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater willDownloadUpdate:(nonnull SUAppcastItem *)item withRequest:(nonnull NSMutableURLRequest *)request
{
    Q_UNUSED(updater)
    Q_UNUSED(request)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Downloading version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater didDownloadUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Downloaded version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater failedToDownloadUpdate:(nonnull SUAppcastItem *)item error:(nonnull NSError *)error
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto errorQstring = QString::fromNSString(error.localizedDescription);
    const auto message = QObject::tr("Error downloading version %1 update: %2", "%1 is version number, %2 is error message").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater willExtractUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Extracting version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater didExtractUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Extracted version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)userDidCancelDownload:(SUUpdater *)updater
{
    Q_UNUSED(updater);
    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:QObject::tr("Update download cancelled.")];
}

@end


namespace OCC {

// Delete ~/Library//Preferences/com.owncloud.desktopclient.plist to re-test
SparkleUpdater::SparkleUpdater(const QUrl& appCastUrl)
    : Updater()
    , _interface(std::make_unique<SparkleInterface>(this))
{
    _interface->delegate = [[NCSparkleUpdaterDelegate alloc] initWithOwner:_interface.get()];
    [_interface->delegate retain];

    _interface->updater = [SUUpdater sharedUpdater];
    [_interface->updater setDelegate:_interface->delegate];
    [_interface->updater setAutomaticallyChecksForUpdates:YES];
    [_interface->updater setAutomaticallyDownloadsUpdates:NO];
    [_interface->updater setSendsSystemProfile:NO];
    [_interface->updater resetUpdateCycle];
    [_interface->updater retain];

    setUpdateUrl(appCastUrl);

    // Sparkle 1.8 required
    const auto userAgentString = QString::fromUtf8(Utility::userAgentString());
    NSString *userAgent = userAgentString.toNSString();
    [_interface->updater setUserAgentString: userAgent];
}

SparkleUpdater::~SparkleUpdater() = default;

void SparkleUpdater::setUpdateUrl(const QUrl &url)
{
    [_interface->updater setFeedURL:url.toNSURL()];
}

bool SparkleUpdater::autoUpdaterAllowed()
{
    // See https://github.com/owncloud/client/issues/2931
    NSString *bundlePath = [[NSBundle mainBundle] bundlePath];
    NSString *expectedPath = [NSString stringWithFormat:@"/Applications/%@", [bundlePath lastPathComponent]];
    if (![expectedPath isEqualTo:bundlePath]) {
        qCWarning(lcUpdater) << "We are not in /Applications, won't check for update!";
        return false;
    }

    if(ConfigFile().skipUpdateCheck()) {
        qCWarning(lcUpdater) << "Auto-updating has been set to skip in nextcloud.cfg, won't check for update.";
        return false;
    }

    return true;
}

void SparkleUpdater::checkForUpdate()
{
    qCDebug(OCC::lcUpdater) << "Checking for updates.";
    if (autoUpdaterAllowed()) {
        [_interface->updater checkForUpdates: NSApp];
    }
}

void SparkleUpdater::backgroundCheckForUpdate()
{
    if (autoUpdaterAllowed() && !ConfigFile().skipUpdateCheck()) {
        qCDebug(OCC::lcUpdater) << "launching background check";
        [_interface->updater checkForUpdatesInBackground];
    } else {
        qCDebug(OCC::lcUpdater) << "not launching background check, auto updater not allowed or update check skipped in config";
    }
}

QString SparkleUpdater::statusString() const
{
    return _statusString;
}

SparkleUpdater::State SparkleUpdater::state() const
{
    return _state;
}

} // namespace OCC

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
        q->_statusString = tr(statusString.toUtf8());
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

- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle
{
    Q_UNUSED(bundle)
    qCDebug(OCC::lcUpdater) << "Updater may check for updates: YES";
    return YES;
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
    const auto message = QStringLiteral("Found a valid update: version %1").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::AwaitingUserInput
              displayStatus:message];
}

// Sent when a valid update is not found.
- (void)updaterDidNotFindUpdate:(SUUpdater *)updater
{
    Q_UNUSED(updater)
    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:QStringLiteral("No valid update found.")];
}

// Sent immediately before installing the specified update.
- (void)updater:(SUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(update.displayVersionString);
    const auto message = QStringLiteral("About to install version %1 update.").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(SUUpdater *)updater didAbortWithError:(NSError *)error
{
    Q_UNUSED(updater)

    const QString message(QStringLiteral("Aborted with error: ") + QString::fromNSString(error.description));
    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(SUUpdater *)updater didFinishLoadingAppcast:(SUAppcast *)appcast
{
    Q_UNUSED(updater)
    Q_UNUSED(appcast)
    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:QStringLiteral("Finished loading appcast.")];
}

- (void)updater:(SUUpdater *)updater didDismissUpdateAlertPermanently:(BOOL)permanently forItem:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto permanencyString = permanently ? QStringLiteral("Permanently") : QStringLiteral("Temporarily");
    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QStringLiteral("%1 dismissed %2 update").arg(permanencyString, versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater userDidSkipThisVersion:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QStringLiteral("Update %1 will not be applied as it was chosen to be skipped.").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater willDownloadUpdate:(nonnull SUAppcastItem *)item withRequest:(nonnull NSMutableURLRequest *)request
{
    Q_UNUSED(updater)
    Q_UNUSED(request)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QStringLiteral("Downloading version %1 update.").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater didDownloadUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QStringLiteral("Downloaded version %1 update.").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater failedToDownloadUpdate:(nonnull SUAppcastItem *)item error:(nonnull NSError *)error
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto errorQstring = QString::fromNSString(error.localizedDescription);
    const auto message = QStringLiteral("Error downloading version %1 update: %2").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater willExtractUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QStringLiteral("Extracting version %1 update.").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SUUpdater *)updater didExtractUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QStringLiteral("Extracted version %1 update.").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)userDidCancelDownload:(SUUpdater *)updater
{
    Q_UNUSED(updater);
    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:QStringLiteral("Update download cancelled.")];
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
    NSURL* nsurl = [NSURL URLWithString:url.toString().toNSString()];
    [_interface->updater setFeedURL: nsurl];
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
    qCDebug(OCC::lcUpdater) << "launching background check";
    if (autoUpdaterAllowed()) {
        [_interface->updater checkForUpdatesInBackground];
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

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
        [updaterController release];
        [delegate release];
    }

    void statusChanged(const OCC::SparkleUpdater::State state, const QString &statusString = {})
    {
        q->_state = state;
        q->_statusString = statusString;
        emit q->statusChanged();
    }

    SPUStandardUpdaterController *updaterController;
    NCSparkleUpdaterDelegate *delegate;

private:
    SparkleUpdater * const q;
};


@interface NCSparkleUpdaterDelegate : NSObject <SPUUpdaterDelegate>

@property (readwrite, assign) OCC::SparkleUpdater::SparkleInterface *owner;
@property (readwrite, retain) NSString *feedURLString;

- (instancetype)initWithOwner:(OCC::SparkleUpdater::SparkleInterface *)owner;

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
    const BOOL allowUpdateCheck = OCC::ConfigFile().skipUpdateCheck() ? NO : YES;
    qCInfo(OCC::lcUpdater) << "Updater may check for updates:" << (allowUpdateCheck ? "YES" : "NO");
    return allowUpdateCheck;
}

- (BOOL)updater:(nonnull SPUUpdater *)updater mayPerformUpdateCheck:(SPUUpdateCheck)updateCheck error:(NSError **)error
{
    Q_UNUSED(updater)
    Q_UNUSED(updateCheck)
    return [self backgroundUpdateChecksAllowed];
}

- (void)notifyStateChange:(const OCC::SparkleUpdater::State)state displayStatus:(const QString&)statusString
{
    qCInfo(OCC::lcUpdater) << statusString;
    _owner->statusChanged(state, statusString);
}

// Sent when a valid update is found by the update driver.
- (void)updater:(nonnull SPUUpdater *)updater didFindValidUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)
    Q_UNUSED(item)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Found a valid update: version %1", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::AwaitingUserInput
              displayStatus:message];
}

// Sent when a valid update is not found.
- (void)updaterDidNotFindUpdate:(nonnull SPUUpdater *)updater
{
    Q_UNUSED(updater)
    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:QObject::tr("No valid update found.")];
}

// Sent immediately before installing the specified update.
- (void)updater:(nonnull SPUUpdater *)updater willInstallUpdate:(nonnull SUAppcastItem *)update
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(update.displayVersionString);
    const auto message = QObject::tr("About to install version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SPUUpdater *)updater didAbortWithError:(nonnull NSError *)error
{
    Q_UNUSED(updater)

    const auto errorQstring = QString::fromNSString(error.localizedDescription);
    const auto message = QObject::tr("Aborted with error: %1", "%1 is version number").arg(errorQstring);
    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(nonnull SPUUpdater *)updater didFinishLoadingAppcast:(nonnull SUAppcast *)appcast
{
    Q_UNUSED(updater)
    Q_UNUSED(appcast)
    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:QObject::tr("Finished loading appcast.")];
}

- (void)updater:(nonnull SPUUpdater *)updater
userDidMakeChoice:(SPUUserUpdateChoice)choice
      forUpdate:(nonnull SUAppcastItem *)item
          state:(nonnull SPUUserUpdateState *)state
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    QString message;

    switch(choice) {
        case SPUUserUpdateChoiceSkip:
            message = QObject::tr("Update version %1 will not be applied as it was chosen to be skipped.",  "%1 is version number").arg(versionQstring);
            break;
        case SPUUserUpdateChoiceInstall:
            message = QObject::tr("Update version %1 will be installed.",  "%1 is version number").arg(versionQstring);
            break;
        case SPUUserUpdateChoiceDismiss:
            message = QObject::tr("Update version %1 will not be applied as it was dismissed.",  "%1 is version number").arg(versionQstring);
            break;
    }

    [self notifyStateChange:OCC::SparkleUpdater::State::Idle displayStatus:message];
}

- (void)updater:(nonnull SPUUpdater *)updater willDownloadUpdate:(nonnull SUAppcastItem *)item withRequest:(nonnull NSMutableURLRequest *)request
{
    Q_UNUSED(updater)
    Q_UNUSED(request)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Downloading version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SPUUpdater *)updater didDownloadUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Downloaded version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SPUUpdater *)updater failedToDownloadUpdate:(nonnull SUAppcastItem *)item error:(nonnull NSError *)error
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto errorQstring = QString::fromNSString(error.localizedDescription);
    const auto message = QObject::tr("Error downloading version %1 update: %2", "%1 is version number, %2 is error message").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:message];
}

- (void)updater:(nonnull SPUUpdater *)updater willExtractUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Extracting version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)updater:(nonnull SPUUpdater *)updater didExtractUpdate:(nonnull SUAppcastItem *)item
{
    Q_UNUSED(updater)

    const auto versionQstring = QString::fromNSString(item.displayVersionString);
    const auto message = QObject::tr("Extracted version %1 update.", "%1 is version number").arg(versionQstring);

    [self notifyStateChange:OCC::SparkleUpdater::State::Working
              displayStatus:message];
}

- (void)userDidCancelDownload:(SPUUpdater *)updater
{
    Q_UNUSED(updater);
    [self notifyStateChange:OCC::SparkleUpdater::State::Idle
              displayStatus:QObject::tr("Update download cancelled.")];
}

- (NSString *)feedURLStringForUpdater:(SPUUpdater *)updater
{
    Q_UNUSED(updater)
    return self.feedURLString;
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

    _interface->updaterController =
        [[SPUStandardUpdaterController alloc] initWithStartingUpdater:YES 
                                                      updaterDelegate:_interface->delegate
                                                   userDriverDelegate:nil];
    [_interface->updaterController retain];

    setUpdateUrl(appCastUrl);

    // Sparkle 1.8 required
    const auto userAgentString = QString::fromUtf8(Utility::userAgentString());
    NSString *const userAgent = userAgentString.toNSString();
    _interface->updaterController.updater.userAgentString = userAgent;
}

SparkleUpdater::~SparkleUpdater() = default;

void SparkleUpdater::setUpdateUrl(const QUrl &url)
{
    _interface->delegate.feedURLString = url.toNSURL().absoluteString;
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
    qCInfo(OCC::lcUpdater) << "Checking for updates.";
    if (autoUpdaterAllowed()) {
        [_interface->updaterController.updater checkForUpdates];
    }
}

void SparkleUpdater::backgroundCheckForUpdate()
{
    if (autoUpdaterAllowed() && !ConfigFile().skipUpdateCheck()) {
        qCInfo(OCC::lcUpdater) << "launching background check";
        [_interface->updaterController.updater checkForUpdatesInBackground];
    } else {
        qCInfo(OCC::lcUpdater) << "not launching background check, auto updater not allowed or update check skipped in config";
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

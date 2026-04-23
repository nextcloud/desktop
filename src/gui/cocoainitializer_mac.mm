/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cocoainitializer.h"

#import <Foundation/NSAutoreleasePool.h>
#import <AppKit/NSApplication.h>

#include "application.h"
#include "editlocallymanager.h"
#include "owncloudsetupwizard.h"

#include <QUrlQuery>

/* In theory, we should be able to just capture QFileOpenEvents
 * when we open our custom URLs in our Application class and be
 * done with it, but in practice the QFileOpenEvent often doesn't
 * get sent for our URLs. We have this in place to work around
 * the issue.
 *
 * This class sets a callback selector on URL-related events
 * before the application is fully done launching. This lets us
 * properly receive and process "open url" events even if the
 * client was closed when these events were sent. */

@interface URLEventHandler : NSObject
@end

@implementation URLEventHandler
- (id)init {
    self = [super init];

    if (self) {
        NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
        [defaultCenter addObserver:self
                        selector:@selector(applicationWillFinishLaunching:)
                        name:NSApplicationWillFinishLaunchingNotification
                        object:nil];
    }
    return self;
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification {
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:self
                                                       andSelector:@selector(handleURLEvent:withReplyEvent:)
                                                     forEventClass:kInternetEventClass
                                                        andEventID:kAEGetURL];
}

- (void)handleURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    NSURL* url = [NSURL URLWithString:[[event paramDescriptorForKeyword:keyDirectObject] stringValue]];
    const auto qtUrl = QUrl::fromNSURL(url);
    const auto pathParts = qtUrl.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const auto hasAddAccountHost = qtUrl.host().compare(QStringLiteral("addaccount"), Qt::CaseInsensitive) == 0;
    const auto hasAddAccountPath = !pathParts.isEmpty() && pathParts.constFirst().compare(QStringLiteral("addaccount"), Qt::CaseInsensitive) == 0;
    if (hasAddAccountHost || hasAddAccountPath) {
        const auto urlQuery = QUrlQuery{qtUrl};
        const auto serverUrlRaw = urlQuery.queryItemValue(QStringLiteral("server_url"));
        const auto serverUrl = QUrl::fromUserInput(serverUrlRaw);
        if (serverUrl.isValid() && !serverUrl.host().isEmpty()) {
            OCC::OwncloudSetupWizard::runWizardForLoginFlow(serverUrl, qApp, SLOT(slotownCloudWizardDone(int)));
            return;
        }
    }

    OCC::EditLocallyManager::instance()->handleRequest(qtUrl);
}

@end

namespace OCC {
namespace Mac {

class CocoaInitializer::Private {
public:
    NSAutoreleasePool* autoReleasePool;
    URLEventHandler* handler;
};

CocoaInitializer::CocoaInitializer() {
    d = new CocoaInitializer::Private();
    d->handler = [[URLEventHandler alloc] init];
    NSApplicationLoad();
    d->autoReleasePool = [[NSAutoreleasePool alloc] init];
}

CocoaInitializer::~CocoaInitializer() {
    [d->autoReleasePool release];
    delete d;
}

} // namespace Mac
} // namespace OCC

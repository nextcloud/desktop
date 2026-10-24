/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cocoainitializer.h"

#import <Foundation/NSException.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSThread.h>
#import <AppKit/NSApplication.h>

#include "application.h"

#include <QTimer>

#include <exception>

static_assert(__has_feature(objc_arc), "CocoaInitializer requires ARC.");

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
- (void)invalidate;
@end

@implementation URLEventHandler {
    BOOL _registered;
}
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
    _registered = YES;
}

- (void)invalidate
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (_registered) {
        [[NSAppleEventManager sharedAppleEventManager] removeEventHandlerForEventClass:kInternetEventClass andEventID:kAEGetURL];
        _registered = NO;
    }
}

- (void)handleURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    NSURL* url = [NSURL URLWithString:[[event paramDescriptorForKeyword:keyDirectObject] stringValue]];
    const auto qtUrl = QUrl::fromNSURL(url);
    if (qApp) {
        QTimer::singleShot(0, qApp, [qtUrl] {
            qCInfo(OCC::lcApplication) << "macOS AppleEvent custom URI scheme request:"
                                       << "scheme=" << qtUrl.scheme()
                                       << "host=" << qtUrl.host()
                                       << "path=" << qtUrl.path();
            if (auto application = qobject_cast<OCC::Application *>(qApp)) {
                application->handleUriSchemeRequest(qtUrl);
            } else {
                qCWarning(OCC::lcApplication) << "Could not dispatch macOS AppleEvent custom URI scheme request because qApp is not an Application.";
            }
        });
    } else {
        qCWarning(OCC::lcApplication) << "Could not dispatch macOS AppleEvent custom URI scheme request because qApp is not available yet.";
    }
}

@end

namespace OCC {
namespace Mac {

class CocoaInitializer::Private {
public:
    URLEventHandler* handler = nil;
    id<NSObject> appNapActivity = nil;
};

int CocoaInitializer::run(const std::function<int()> &mainFunction)
{
    Q_ASSERT([NSThread isMainThread]);
    auto result = 0;
    std::exception_ptr exception;
    NSException *nativeException = nil;
    // Leave the pool normally so it drains before either exception is rethrown.
    @autoreleasepool {
        try {
            @try {
                CocoaInitializer initializer;
                result = mainFunction();
            } @catch (NSException *caughtException) {
                nativeException = caughtException;
            }
        } catch (...) {
            exception = std::current_exception();
        }
    }
    if (nativeException != nil) {
        @throw nativeException;
    }
    if (exception) {
        std::rethrow_exception(exception);
    }
    return result;
}

CocoaInitializer::CocoaInitializer() {
    d = new CocoaInitializer::Private();
    d->handler = [[URLEventHandler alloc] init];
    NSApplicationLoad();

    // As an LSUIElement agent we usually have no visible window, which makes macOS
    // throttle us via App Nap and lets RunningBoard suspend (and later jetsam-reap)
    // the process while it is idle in the background. Holding an NSActivityBackground
    // assertion for our whole lifetime keeps the process out of App Nap so syncing
    // continues; it still allows normal idle system/display sleep.
    NSProcessInfo *const processInfo = [NSProcessInfo processInfo];
    d->appNapActivity = [processInfo beginActivityWithOptions:NSActivityBackground
                                                        reason:@"Keep syncing while running in the background"];
}

CocoaInitializer::~CocoaInitializer() {
    [d->handler invalidate];
    if (d->appNapActivity) {
        [[NSProcessInfo processInfo] endActivity:d->appNapActivity];
    }
    delete d;
}

} // namespace Mac
} // namespace OCC

#include <QString>

#import <Foundation/NSString.h>
#import <Foundation/NSUserNotification.h>
#import <dispatch/dispatch.h>

@interface OurDelegate : NSObject <NSUserNotificationCenterDelegate>

- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center
     shouldPresentNotification:(NSUserNotification *)notification;

@end

@implementation OurDelegate

// Always show, even if app is active at the moment.
- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center
     shouldPresentNotification:(NSUserNotification *)notification
{
    Q_UNUSED(center)
    Q_UNUSED(notification)

    return YES;
}

@end

namespace OCC {

void *createOsXNotificationCenterDelegate()
{
    auto delegate = [[OurDelegate alloc] init];
    [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:delegate];

    return delegate;
}

void releaseOsXNotificationCenterDelegate(void *delegate)
{
    [static_cast<OurDelegate *>(delegate) release];
}

void sendOsXUserNotification(const QString &title, const QString &message)
{
    @autoreleasepool {
        NSUserNotification *notification = [[[NSUserNotification alloc] init] autorelease];
        [notification setTitle:[NSString stringWithUTF8String:title.toUtf8().data()]];
        [notification setInformativeText:[NSString stringWithUTF8String:message.toUtf8().data()]];
        [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:notification];
    }
}

} // namespace OCC

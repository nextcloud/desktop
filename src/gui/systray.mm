#include <QString>
#import <Cocoa/Cocoa.h>

@interface NotificationCenterDelegate : NSObject
@end
@implementation NotificationCenterDelegate
// Always show, even if app is active at the moment.
- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center
     shouldPresentNotification:(NSUserNotification *)notification
{
    Q_UNUSED(center);
    Q_UNUSED(notification);
    return YES;
}
@end

namespace OCC {

bool canOsXSendUserNotification()
{
    return NSClassFromString(@"NSUserNotificationCenter") != nil;
}

void sendOsXUserNotification(const QString &title, const QString &message)
{
    Class cuserNotificationCenter = NSClassFromString(@"NSUserNotificationCenter");
    id userNotificationCenter = [cuserNotificationCenter defaultUserNotificationCenter];

    static dispatch_once_t once;
    dispatch_once(&once, ^{
            id delegate = [[NotificationCenterDelegate alloc] init];
            [userNotificationCenter setDelegate:delegate];
    });

    Class cuserNotification = NSClassFromString(@"NSUserNotification");
    id notification = [[cuserNotification alloc] init];
    [notification setTitle:[NSString stringWithUTF8String:title.toUtf8().data()]];
    [notification setInformativeText:[NSString stringWithUTF8String:message.toUtf8().data()]];

    [userNotificationCenter deliverNotification:notification];
    [notification release];
}

}

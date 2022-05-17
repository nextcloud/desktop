#include "QtCore/qurl.h"
#include "config.h"
#include <QString>
#include <QWindow>
#include <QLoggingCategory>

#import <Cocoa/Cocoa.h>
#import <UserNotifications/UserNotifications.h>

Q_LOGGING_CATEGORY(lcMacSystray, "nextcloud.gui.macsystray")

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

enum MacNotificationAuthorizationOptions {
    Default = 0,
    Provisional
};

bool canOsXSendUserNotification()
{
    return NSClassFromString(@"NSUserNotificationCenter") != nil;
}

double statusBarThickness()
{
    return [NSStatusBar systemStatusBar].thickness;
}

void checkNotificationAuth(MacNotificationAuthorizationOptions additionalAuthOption = MacNotificationAuthorizationOptions::Provisional)
{
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    UNAuthorizationOptions authOptions = UNAuthorizationOptionAlert + UNAuthorizationOptionSound;

    if(additionalAuthOption == MacNotificationAuthorizationOptions::Provisional) {
        authOptions += UNAuthorizationOptionProvisional;
    }

    [center requestAuthorizationWithOptions:(authOptions)
        completionHandler:^(BOOL granted, NSError * _Nullable error) {
            // Enable or disable features based on authorization.
            if(granted) {
                qCDebug(lcMacSystray) << "Authorization for notifications has been granted, can display notifications.";
            } else {
                qCDebug(lcMacSystray) << "Authorization for notifications not granted.";
                if(error) {
                    QString errorDescription([error.localizedDescription UTF8String]);
                    qCDebug(lcMacSystray) << "Error from notification center: " << errorDescription;
                }
            }
    }];
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

void setTrayWindowLevelAndVisibleOnAllSpaces(QWindow *window)
{
    NSView *nativeView = (NSView *)window->winId();
    NSWindow *nativeWindow = (NSWindow *)[nativeView window];
    [nativeWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorIgnoresCycle |
                  NSWindowCollectionBehaviorTransient];
    [nativeWindow setLevel:NSMainMenuWindowLevel];
}

}

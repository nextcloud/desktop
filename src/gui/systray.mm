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
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    willPresentNotification:(UNNotification *)notification
    withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler
{
    if (@available(macOS 11.0, *)) {
        completionHandler(UNNotificationPresentationOptionSound + UNNotificationPresentationOptionBanner);
    } else {
        completionHandler(UNNotificationPresentationOptionSound + UNNotificationPresentationOptionAlert);
    }
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    didReceiveNotificationResponse:(UNNotificationResponse *)response
    withCompletionHandler:(void (^)(void))completionHandler
{
    qCDebug(lcMacSystray()) << "Received notification with category identifier:" << response.notification.request.content.categoryIdentifier
                            << "and action identifier" << response.actionIdentifier;
    UNNotificationContent* content = response.notification.request.content;
    if ([content.categoryIdentifier isEqualToString:@"UPDATE"]) {

        if ([response.actionIdentifier isEqualToString:@"DOWNLOAD_ACTION"] || [response.actionIdentifier isEqualToString:UNNotificationDefaultActionIdentifier])
        {
            qCDebug(lcMacSystray()) << "Opening update download url in browser.";
            [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[content.userInfo objectForKey:@"webUrl"]]];
        }
    }

    completionHandler();
}
@end

namespace OCC {

enum MacNotificationAuthorizationOptions {
    Default = 0,
    Provisional
};

double statusBarThickness()
{
    return [NSStatusBar systemStatusBar].thickness;
}

// TODO: Get this to actually check for permissions
bool canOsXSendUserNotification()
{
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    return center != nil;
}

void registerNotificationCategories(const QString &localisedDownloadString) {
    UNNotificationCategory* generalCategory = [UNNotificationCategory
          categoryWithIdentifier:@"GENERAL"
          actions:@[]
          intentIdentifiers:@[]
          options:UNNotificationCategoryOptionCustomDismissAction];

    // Create the custom actions for update notifications.
    UNNotificationAction* downloadAction = [UNNotificationAction
          actionWithIdentifier:@"DOWNLOAD_ACTION"
          title:localisedDownloadString.toNSString()
          options:UNNotificationActionOptionNone];

    // Create the category with the custom actions.
    UNNotificationCategory* updateCategory = [UNNotificationCategory
          categoryWithIdentifier:@"UPDATE"
          actions:@[downloadAction]
          intentIdentifiers:@[]
          options:UNNotificationCategoryOptionNone];

    [[UNUserNotificationCenter currentNotificationCenter] setNotificationCategories:[NSSet setWithObjects:generalCategory, updateCategory, nil]];
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

void setUserNotificationCenterDelegate()
{
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];

    static dispatch_once_t once;
    dispatch_once(&once, ^{
            id delegate = [[NotificationCenterDelegate alloc] init];
            [center setDelegate:delegate];
    });
}

UNMutableNotificationContent* basicNotificationContent(const QString &title, const QString &message)
{
    UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
    content.title = title.toNSString();
    content.body = message.toNSString();
    content.sound = [UNNotificationSound defaultSound];

    return content;
}

void sendOsXUserNotification(const QString &title, const QString &message)
{
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    checkNotificationAuth();

    UNMutableNotificationContent* content = basicNotificationContent(title, message);
    content.categoryIdentifier = @"GENERAL";

    UNTimeIntervalNotificationTrigger* trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1 repeats: NO];
    UNNotificationRequest* request = [UNNotificationRequest requestWithIdentifier:@"NCUserNotification" content:content trigger:trigger];

    [center addNotificationRequest:request withCompletionHandler:nil];
}

void sendOsXUpdateNotification(const QString &title, const QString &message, const QUrl &webUrl)
{
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    checkNotificationAuth();

    UNMutableNotificationContent* content = basicNotificationContent(title, message);
    content.categoryIdentifier = @"UPDATE";
    content.userInfo = [NSDictionary dictionaryWithObject:[webUrl.toNSURL() absoluteString] forKey:@"webUrl"];

    UNTimeIntervalNotificationTrigger* trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1 repeats: NO];
    UNNotificationRequest* request = [UNNotificationRequest requestWithIdentifier:@"NCUpdateNotification" content:content trigger:trigger];

    [center addNotificationRequest:request withCompletionHandler:nil];
}

void setTrayWindowLevelAndVisibleOnAllSpaces(QWindow *window)
{
    NSView *nativeView = (NSView *)window->winId();
    NSWindow *nativeWindow = (NSWindow *)[nativeView window];
    [nativeWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorIgnoresCycle |
                  NSWindowCollectionBehaviorTransient];
    [nativeWindow setLevel:NSMainMenuWindowLevel];
}

bool osXInDarkMode()
{
    NSString *osxMode = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
    return [osxMode containsString:@"Dark"];
}

}


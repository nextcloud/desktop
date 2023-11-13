/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include <QLoggingCategory>
#include <QString>
#include <QUrl>

#import <Cocoa/Cocoa.h>
#import <UserNotifications/UserNotifications.h>

#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "config.h"
#include "systray.h"
#include "tray/talkreply.h"

Q_LOGGING_CATEGORY(lcMacSystrayUserNotifications, "nextcloud.gui.macsystrayusernotifications")

/************************* Private utility functions *************************/

namespace {

void sendTalkReply(UNNotificationResponse *response, UNNotificationContent* content)
{
    if (!response || !content) {
        qCWarning(lcMacSystrayUserNotifications) << "Invalid notification response or content."
                                                 << "Can't send talk reply.";
        return;
    }

    UNTextInputNotificationResponse * const textInputResponse = (UNTextInputNotificationResponse*)response;

    if (!textInputResponse) {
        qCWarning(lcMacSystrayUserNotifications) << "Notification response was not a text input response."
                                                 << "Can't send talk reply.";
        return;
    }

    NSString * const reply = textInputResponse.userText;
    NSString * const token = [content.userInfo objectForKey:@"token"];
    NSString * const account = [content.userInfo objectForKey:@"account"];
    NSString * const replyTo = [content.userInfo objectForKey:@"replyTo"];

    const auto qReply = QString::fromNSString(reply);
    const auto qReplyTo = QString::fromNSString(replyTo);
    const auto qToken = QString::fromNSString(token);
    const auto qAccount = QString::fromNSString(account);

    const auto accountState = OCC::AccountManager::instance()->accountFromUserId(qAccount);

    if (!accountState) {
        qCWarning(lcMacSystrayUserNotifications) << "Could not find account matching" << qAccount
                                                 << "Can't send talk reply.";
        return;
    }

    qCDebug(lcMacSystrayUserNotifications) << "Sending talk reply from macOS notification."
                                           << "Reply is:" << qReply
                                           << "Replying to:" << qReplyTo
                                           << "Token:" << qToken
                                           << "Account:" << qAccount;

    // OCC::TalkReply deletes itself once it's done, fire and forget
    const auto talkReply = new OCC::TalkReply(accountState.data(), OCC::Systray::instance());
    talkReply->sendReplyMessage(qToken, qReply, qReplyTo);
}

} // anonymous namespace

/**************************** Objective-C classes ****************************/

@interface NotificationCenterDelegate : NSObject
@end
@implementation NotificationCenterDelegate

// Always show, even if app is active at the moment.
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    willPresentNotification:(UNNotification *)notification
    withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_11_0
        completionHandler(UNNotificationPresentationOptionSound + UNNotificationPresentationOptionBanner);
#else
        completionHandler(UNNotificationPresentationOptionSound + UNNotificationPresentationOptionAlert);
#endif
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
didReceiveNotificationResponse:(UNNotificationResponse *)response
         withCompletionHandler:(void (^)(void))completionHandler
{
    qCDebug(lcMacSystrayUserNotifications) << "Received notification with category identifier:"
                                           << response.notification.request.content.categoryIdentifier
                                           << "and action identifier"
                                           << response.actionIdentifier;

    UNNotificationContent * const content = response.notification.request.content;
    if ([content.categoryIdentifier isEqualToString:@"UPDATE"]) {

        if ([response.actionIdentifier isEqualToString:@"DOWNLOAD_ACTION"] || [response.actionIdentifier isEqualToString:UNNotificationDefaultActionIdentifier]) {
            qCDebug(lcMacSystrayUserNotifications) << "Opening update download url in browser.";
            [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[content.userInfo objectForKey:@"webUrl"]]];
        }
    } else if ([content.categoryIdentifier isEqualToString:@"TALK_MESSAGE"]) {

        if ([response.actionIdentifier isEqualToString:@"TALK_REPLY_ACTION"]) {
            sendTalkReply(response, content);
        }
    }

    completionHandler();
}
@end

/********************* Methods accessible to C++ Systray *********************/

namespace OCC {

// TODO: Get this to actually check for permissions
bool canOsXSendUserNotification()
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;
    return center != nil;
}

void registerNotificationCategories(const QString &localisedDownloadString) {
    UNNotificationCategory * const generalCategory = [UNNotificationCategory
        categoryWithIdentifier:@"GENERAL"
        actions:@[]
        intentIdentifiers:@[]
        options:UNNotificationCategoryOptionCustomDismissAction];

    // Create the custom actions for update notifications.
    UNNotificationAction * const downloadAction = [UNNotificationAction
        actionWithIdentifier:@"DOWNLOAD_ACTION"
        title:localisedDownloadString.toNSString()
        options:UNNotificationActionOptionNone];

    // Create the category with the custom actions.
    UNNotificationCategory * const updateCategory = [UNNotificationCategory
        categoryWithIdentifier:@"UPDATE"
        actions:@[downloadAction]
        intentIdentifiers:@[]
        options:UNNotificationCategoryOptionNone];

    // Create the custom action for talk notifications
    UNTextInputNotificationAction * const talkReplyAction = [UNTextInputNotificationAction
        actionWithIdentifier:@"TALK_REPLY_ACTION"
        title:QObject::tr("Reply").toNSString()
        options:UNNotificationActionOptionNone
        textInputButtonTitle:QObject::tr("Reply").toNSString()
        textInputPlaceholder:QObject::tr("Send a Nextcloud Talk reply").toNSString()];

    UNNotificationCategory * const talkReplyCategory = [UNNotificationCategory
        categoryWithIdentifier:@"TALK_MESSAGE"
        actions:@[talkReplyAction]
        intentIdentifiers:@[]
        options:UNNotificationCategoryOptionNone];

    [UNUserNotificationCenter.currentNotificationCenter setNotificationCategories:[NSSet setWithObjects:generalCategory, updateCategory, talkReplyCategory, nil]];
}

void checkNotificationAuth(MacNotificationAuthorizationOptions additionalAuthOption)
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;
    UNAuthorizationOptions authOptions = UNAuthorizationOptionAlert + UNAuthorizationOptionSound;

    if(additionalAuthOption == MacNotificationAuthorizationOptions::Provisional) {
        authOptions += UNAuthorizationOptionProvisional;
    }

    [center requestAuthorizationWithOptions:(authOptions) completionHandler:^(BOOL granted, NSError * _Nullable error) {
        // Enable or disable features based on authorization.
        if (granted) {
            qCDebug(lcMacSystrayUserNotifications) << "Authorization for notifications has been granted, can display notifications.";
        } else {
            qCDebug(lcMacSystrayUserNotifications) << "Authorization for notifications not granted.";
            if (error) {
                const auto errorDescription = QString::fromNSString(error.localizedDescription);
                qCDebug(lcMacSystrayUserNotifications) << "Error from notification center: " << errorDescription;
            }
        }
    }];
}

void setUserNotificationCenterDelegate()
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        id delegate = [[NotificationCenterDelegate alloc] init];
        [center setDelegate:delegate];
    });
}

UNMutableNotificationContent* basicNotificationContent(const QString &title, const QString &message)
{
    UNMutableNotificationContent * const content = [[UNMutableNotificationContent alloc] init];
    content.title = title.toNSString();
    content.body = message.toNSString();
    content.sound = [UNNotificationSound defaultSound];

    return content;
}

void sendOsXUserNotification(const QString &title, const QString &message)
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;
    checkNotificationAuth();

    UNMutableNotificationContent * const content = basicNotificationContent(title, message);
    content.categoryIdentifier = @"GENERAL";

    UNTimeIntervalNotificationTrigger * const trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1 repeats:NO];
    UNNotificationRequest * const request = [UNNotificationRequest requestWithIdentifier:@"NCUserNotification" content:content trigger:trigger];

    [center addNotificationRequest:request withCompletionHandler:nil];
}

void sendOsXUpdateNotification(const QString &title, const QString &message, const QUrl &webUrl)
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;
    checkNotificationAuth();

    UNMutableNotificationContent * const content = basicNotificationContent(title, message);
    content.categoryIdentifier = @"UPDATE";
    content.userInfo = [NSDictionary dictionaryWithObject:[webUrl.toNSURL() absoluteString] forKey:@"webUrl"];

    UNTimeIntervalNotificationTrigger * const trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1 repeats:NO];
    UNNotificationRequest * const request = [UNNotificationRequest requestWithIdentifier:@"NCUpdateNotification" content:content trigger:trigger];

    [center addNotificationRequest:request withCompletionHandler:nil];
}

void sendOsXTalkNotification(const QString &title, const QString &message, const QString &token, const QString &replyTo, const AccountStatePtr accountState)
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;
    checkNotificationAuth();

    if (!accountState || !accountState->account()) {
        sendOsXUserNotification(title, message);
        return;
    }

    NSString * const accountNS = accountState->account()->displayName().toNSString();
    NSString * const tokenNS = token.toNSString();
    NSString * const replyToNS = replyTo.toNSString();

    UNMutableNotificationContent * const content = basicNotificationContent(title, message);
    content.categoryIdentifier = @"TALK_MESSAGE";
    content.userInfo = [NSDictionary dictionaryWithObjects:@[accountNS, tokenNS, replyToNS] forKeys:@[@"account", @"token", @"replyTo"]];

    UNTimeIntervalNotificationTrigger * const trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1 repeats:NO];
    UNNotificationRequest * const request = [UNNotificationRequest requestWithIdentifier:@"NCTalkMessageNotification" content:content trigger:trigger];

    [center addNotificationRequest:request withCompletionHandler:nil];
}

} // OCC namespace

/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QLoggingCategory>
#include <QCryptographicHash>
#include <QString>
#include <QUrl>

#include <functional>

#import <Cocoa/Cocoa.h>
#import <UserNotifications/UserNotifications.h>

#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "config.h"
#include "notifications/macnotificationcenter.h"
#include "notifications/notificationmanager.h"
#import "notifications/ncnotificationcenterdelegate.h"

Q_LOGGING_CATEGORY(lcMacNotificationCenter, "nextcloud.gui.macnotificationcenter")

/************************* Private utility functions *************************/

namespace {

constexpr auto maximumNativeNotificationActions = 4;

NSString * const serverCategoryPrefix = @"NEXTCLOUD_SERVER_";
NSString * const serverActionPrefix = @"NEXTCLOUD_SERVER_ACTION_";
NSString * const legacyTalkCategoryIdentifier = @"TALK_MESSAGE";
NSString * const accountIdUserInfoKey = @"accountId";
NSString * const notificationIdUserInfoKey = @"notificationId";
NSString * const responseActionsUserInfoKey = @"responseActions";
NSString * const actionTypeUserInfoKey = @"actionType";
NSString * const actionLinkUserInfoKey = @"link";
NSString * const actionVerbUserInfoKey = @"verb";

NSMutableDictionary *notificationCategoryRegistry()
{
    static NSMutableDictionary *registry = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        registry = [[NSMutableDictionary alloc] init];
    });
    return registry;
}

void setRegisteredNotificationCategories()
{
    auto * const registry = notificationCategoryRegistry();
    NSSet *categories = nil;
    @synchronized (registry) {
        categories = [NSSet setWithArray:registry.allValues];
    }
    [UNUserNotificationCenter.currentNotificationCenter setNotificationCategories:categories];
}

void registerNativeNotificationCategory(UNNotificationCategory *category)
{
    if (!category) {
        return;
    }

    auto * const registry = notificationCategoryRegistry();
    @synchronized (registry) {
        [registry setObject:category forKey:category.identifier];
    }
    setRegisteredNotificationCategories();
}

void seedNativeNotificationCategories(NSArray *categories)
{
    auto * const center = UNUserNotificationCenter.currentNotificationCenter;
    auto * const registry = notificationCategoryRegistry();
    @synchronized (registry) {
        for (UNNotificationCategory *category in categories) {
            [registry setObject:category forKey:category.identifier];
        }
    }

    [center getNotificationCategoriesWithCompletionHandler:^(NSSet<UNNotificationCategory *> *registeredCategories) {
        @synchronized (registry) {
            for (UNNotificationCategory *category in registeredCategories) {
                if ([category.identifier isEqualToString:legacyTalkCategoryIdentifier]) {
                    continue;
                }
                if (![registry objectForKey:category.identifier]) {
                    [registry setObject:category forKey:category.identifier];
                }
            }
        }
        setRegisteredNotificationCategories();
    }];
}

QString nativeServerNotificationIdentifier(const QString &accountId, const qint64 notificationId)
{
    auto identity = accountId.toUtf8();
    identity += ':';
    identity += QByteArray::number(notificationId);
    const auto digest = QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex();
    return QStringLiteral("NextcloudServerNotification-%1").arg(QString::fromLatin1(digest));
}

QVariantList nativeNotificationActions(const OCC::Activity &activity)
{
    const auto previewActions = activity.notificationPreviewActions();
    if (previewActions.size() <= maximumNativeNotificationActions) {
        return previewActions;
    }

    auto selectedActions = QVariantList{};
    auto dismissAction = QVariant{};
    for (const auto &action : previewActions) {
        const auto actionMap = action.toMap();
        if (actionMap.value(QStringLiteral("actionType")).toString() == QStringLiteral("dismiss")) {
            dismissAction = action;
            continue;
        }
        if (selectedActions.size() < maximumNativeNotificationActions - (dismissAction.isValid() ? 1 : 0)) {
            selectedActions.push_back(action);
        }
    }

    if (dismissAction.isValid()) {
        while (selectedActions.size() >= maximumNativeNotificationActions) {
            selectedActions.removeLast();
        }
        selectedActions.push_back(dismissAction);
    }

    return selectedActions;
}

void performOnMainThread(dispatch_block_t action)
{
    if ([NSThread isMainThread]) {
        action();
        return;
    }
    dispatch_sync(dispatch_get_main_queue(), action);
}

void performWithNotificationManager(
    const QString &accountId, const std::function<void(OCC::NotificationManager *)> &action)
{
    const auto capturedAccountId = accountId;
    const auto capturedAction = action;
    performOnMainThread(^{
        const auto notificationManager = OCC::NotificationManager::forAccountId(capturedAccountId);
        if (!notificationManager) {
            qCWarning(lcMacNotificationCenter) << "Could not find notification manager for native response.";
            return;
        }
        capturedAction(notificationManager);
    });
}

QHash<QString, quint64> &serverNotificationReconciliationGenerations()
{
    static auto generations = QHash<QString, quint64>{};
    return generations;
}

quint64 beginServerNotificationReconciliation(const QString &accountId)
{
    auto &generation = serverNotificationReconciliationGenerations()[accountId];
    ++generation;
    return generation;
}

bool isCurrentServerNotificationReconciliation(const QString &accountId, const quint64 generation)
{
    return serverNotificationReconciliationGenerations().value(accountId) == generation;
}

NSArray *staleServerNotificationIdentifiers(
    NSArray<UNNotificationRequest *> *requests, const QString &accountId, const QSet<qint64> &activeNotificationIds)
{
    auto * const staleIdentifiers = [NSMutableArray array];
    for (UNNotificationRequest *request in requests) {
        const auto content = request.content;
        if (![content.categoryIdentifier hasPrefix:serverCategoryPrefix]) {
            continue;
        }

        NSString * const requestAccountId = [content.userInfo objectForKey:accountIdUserInfoKey];
        NSNumber * const notificationId = [content.userInfo objectForKey:notificationIdUserInfoKey];
        if ([requestAccountId isEqualToString:accountId.toNSString()]
            && [notificationId isKindOfClass:[NSNumber class]]
            && !activeNotificationIds.contains(notificationId.longLongValue)) {
            [staleIdentifiers addObject:request.identifier];
        }
    }
    return staleIdentifiers;
}

void removePendingAndDeliveredNotifications(NSArray *identifiers)
{
    if (identifiers.count == 0) {
        return;
    }

    auto * const center = UNUserNotificationCenter.currentNotificationCenter;
    [center removePendingNotificationRequestsWithIdentifiers:identifiers];
    [center removeDeliveredNotificationsWithIdentifiers:identifiers];
}

bool handleServerNotificationResponse(UNNotificationResponse *response, UNNotificationContent *content)
{
    if (![content.categoryIdentifier hasPrefix:serverCategoryPrefix]) {
        return false;
    }

    NSString * const accountId = [content.userInfo objectForKey:accountIdUserInfoKey];
    NSNumber * const notificationId = [content.userInfo objectForKey:notificationIdUserInfoKey];
    if (![accountId isKindOfClass:[NSString class]] || ![notificationId isKindOfClass:[NSNumber class]]) {
        qCWarning(lcMacNotificationCenter) << "Native server notification is missing its stable identity.";
        return true;
    }

    const auto qAccountId = QString::fromNSString(accountId);
    const auto qNotificationId = notificationId.longLongValue;

    if ([response.actionIdentifier isEqualToString:UNNotificationDismissActionIdentifier]) {
        performWithNotificationManager(qAccountId, [qNotificationId](OCC::NotificationManager *notificationManager) {
            notificationManager->dismissServerNotification(qNotificationId);
        });
        return true;
    }

    if ([response.actionIdentifier isEqualToString:UNNotificationDefaultActionIdentifier]) {
        performWithNotificationManager(qAccountId, [](OCC::NotificationManager *notificationManager) {
            notificationManager->showActivities();
        });
        return true;
    }

    NSDictionary * const responseActions = [content.userInfo objectForKey:responseActionsUserInfoKey];
    NSDictionary * const action = [responseActions objectForKey:response.actionIdentifier];
    NSString * const actionType = [action objectForKey:actionTypeUserInfoKey];
    if (![actionType isKindOfClass:[NSString class]]) {
        qCWarning(lcMacNotificationCenter) << "Native server notification action has no dispatch metadata.";
        return true;
    }

    const auto qActionType = QString::fromNSString(actionType);
    if (qActionType == QStringLiteral("dismiss")) {
        performWithNotificationManager(qAccountId, [qNotificationId](OCC::NotificationManager *notificationManager) {
            notificationManager->dismissServerNotification(qNotificationId);
        });
    } else if (qActionType == QStringLiteral("reply")) {
        if (![response isKindOfClass:[UNTextInputNotificationResponse class]]) {
            qCWarning(lcMacNotificationCenter) << "Native Talk reply was not a text input response.";
            return true;
        }

        UNTextInputNotificationResponse * const textResponse = (UNTextInputNotificationResponse *)response;
        const auto qReply = QString::fromNSString(textResponse.userText);
        const auto qToken = QString::fromNSString([content.userInfo objectForKey:@"token"]);
        const auto qReplyTo = QString::fromNSString([content.userInfo objectForKey:@"replyTo"]);
        performWithNotificationManager(qAccountId, [qReply, qToken, qReplyTo](OCC::NotificationManager *notificationManager) {
            notificationManager->sendTalkReply(qReply, qToken, qReplyTo);
        });
    } else if (qActionType == QStringLiteral("trigger")) {
        const auto qLink = QString::fromNSString([action objectForKey:actionLinkUserInfoKey]);
        const auto qVerb = QString::fromNSString([action objectForKey:actionVerbUserInfoKey]).toUtf8();
        performWithNotificationManager(qAccountId, [qNotificationId, qLink, qVerb](OCC::NotificationManager *notificationManager) {
            notificationManager->triggerServerNotificationAction(qNotificationId, qLink, qVerb);
        });
    } else if (qActionType == QStringLiteral("openActivities")) {
        performWithNotificationManager(qAccountId, [](OCC::NotificationManager *notificationManager) {
            notificationManager->showActivities();
        });
    }

    return true;
}

} // anonymous namespace

/********************* Methods accessible to C++ callers *********************/

namespace OCC::MacNotificationCenter {

enum class AuthorizationOptions {
    Default,
    Provisional,
};

void willPresentNotification(void (^completionHandler)(UNNotificationPresentationOptions options))
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_11_0
    completionHandler(UNNotificationPresentationOptionSound + UNNotificationPresentationOptionBanner);
#else
    completionHandler(UNNotificationPresentationOptionSound + UNNotificationPresentationOptionAlert);
#endif
}

void didReceiveNotificationResponse(UNNotificationResponse *response, void (^completionHandler)(void))
{
    qCDebug(lcMacNotificationCenter) << "Received notification with category identifier:"
                                     << response.notification.request.content.categoryIdentifier
                                     << "and action identifier"
                                     << response.actionIdentifier;

    UNNotificationContent * const content = response.notification.request.content;
    if (handleServerNotificationResponse(response, content)) {
        completionHandler();
        return;
    }

    if ([content.categoryIdentifier isEqualToString:@"UPDATE"]) {
        if ([response.actionIdentifier isEqualToString:@"DOWNLOAD_ACTION"]
            || [response.actionIdentifier isEqualToString:UNNotificationDefaultActionIdentifier]) {
            qCDebug(lcMacNotificationCenter) << "Opening update download url in browser.";
            [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[content.userInfo objectForKey:@"webUrl"]]];
        }
    }

    completionHandler();
}

// TODO: Get this to actually check for permissions
bool canSendNotification()
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;
    return center != nil;
}

void registerBaseNotificationCategories(const QString &localisedDownloadString)
{
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

    seedNativeNotificationCategories(@[generalCategory, updateCategory]);
}

void requestAuthorization(const AuthorizationOptions additionalAuthOption = AuthorizationOptions::Provisional)
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;
    UNAuthorizationOptions authOptions = UNAuthorizationOptionAlert + UNAuthorizationOptionSound;

    if (additionalAuthOption == AuthorizationOptions::Provisional) {
        authOptions += UNAuthorizationOptionProvisional;
    }

    [center requestAuthorizationWithOptions:(authOptions) completionHandler:^(BOOL granted, NSError * _Nullable error) {
        // Enable or disable features based on authorization.
        if (granted) {
            qCDebug(lcMacNotificationCenter) << "Authorization for notifications has been granted, can display notifications.";
        } else {
            qCDebug(lcMacNotificationCenter) << "Authorization for notifications not granted.";
            if (error) {
                const auto errorDescription = QString::fromNSString(error.localizedDescription);
                qCWarning(lcMacNotificationCenter) << "Error from notification center: " << errorDescription;
            }
        }
    }];
}

void setNotificationCenterDelegate()
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        id delegate = [[NCNotificationCenterDelegate alloc] init];
        [center setDelegate:delegate];
    });
}

void initialize(const QString &localizedDownloadString)
{
    setNotificationCenterDelegate();
    requestAuthorization(AuthorizationOptions::Default);
    registerBaseNotificationCategories(localizedDownloadString);
}

UNMutableNotificationContent *basicNotificationContent(const QString &title, const QString &message)
{
    UNMutableNotificationContent * const content = [[[UNMutableNotificationContent alloc] init] autorelease];
    content.title = title.toNSString();
    content.body = message.toNSString();
    content.sound = [UNNotificationSound defaultSound];

    return content;
}

void sendNotification(const QString &title, const QString &message)
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;
    requestAuthorization();

    UNMutableNotificationContent * const content = basicNotificationContent(title, message);
    content.categoryIdentifier = @"GENERAL";

    UNTimeIntervalNotificationTrigger * const trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1 repeats:NO];
    UNNotificationRequest * const request = [UNNotificationRequest requestWithIdentifier:@"NCUserNotification" content:content trigger:trigger];

    [center addNotificationRequest:request withCompletionHandler:nil];
}

void sendUpdateNotification(const QString &title, const QString &message, const QUrl &webUrl)
{
    UNUserNotificationCenter * const center = UNUserNotificationCenter.currentNotificationCenter;
    requestAuthorization();

    UNMutableNotificationContent * const content = basicNotificationContent(title, message);
    content.categoryIdentifier = @"UPDATE";
    content.userInfo = [NSDictionary dictionaryWithObject:[webUrl.toNSURL() absoluteString] forKey:@"webUrl"];

    UNTimeIntervalNotificationTrigger * const trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1 repeats:NO];
    UNNotificationRequest * const request = [UNNotificationRequest requestWithIdentifier:@"NCUpdateNotification" content:content trigger:trigger];

    [center addNotificationRequest:request withCompletionHandler:nil];
}

void sendServerNotification(const Activity &activity, const AccountStatePtr &accountState)
{
    if (!accountState || !accountState->account()) {
        const auto previewText = activity.notificationPreviewText();
        sendNotification(previewText.title, previewText.subtitle);
        return;
    }

    auto * const center = UNUserNotificationCenter.currentNotificationCenter;
    requestAuthorization();

    const auto previewText = activity.notificationPreviewText();
    auto message = previewText.subtitle;
    if (message.isEmpty() && AccountManager::instance()->accounts().size() > 1) {
        message = activity._accName;
    }

    auto * const nativeActions = [NSMutableArray array];
    auto * const responseActions = [NSMutableDictionary dictionary];
    auto categorySignature = QByteArray{};
    const auto actions = nativeNotificationActions(activity);
    for (auto nativeActionIndex = 0; nativeActionIndex < actions.size(); ++nativeActionIndex) {
        const auto action = actions.at(nativeActionIndex).toMap();
        const auto label = action.value(QStringLiteral("label")).toString();
        const auto actionType = action.value(QStringLiteral("actionType")).toString();
        const auto actionIndex = action.value(QStringLiteral("actionIndex")).toInt();
        const auto verb = action.value(QStringLiteral("verb")).toString().toUtf8();
        if (label.isEmpty()) {
            continue;
        }

        NSString * const identifier = [NSString stringWithFormat:@"%@%d", serverActionPrefix, nativeActionIndex];
        auto nativeActionType = actionType;
        UNNotificationAction *nativeAction = nil;
        const auto isInlineReply = verb == QByteArrayLiteral("REPLY")
            && !activity._talkNotificationData.messageId.isEmpty();
        if (isInlineReply) {
            nativeActionType = QStringLiteral("reply");
            nativeAction = [UNTextInputNotificationAction
                actionWithIdentifier:identifier
                title:label.toNSString()
                options:UNNotificationActionOptionNone
                textInputButtonTitle:QObject::tr("Reply").toNSString()
                textInputPlaceholder:QObject::tr("Send a Nextcloud Talk reply").toNSString()];
        } else {
            auto options = UNNotificationActionOptionNone;
            if (verb == QByteArrayLiteral("DELETE")) {
                options = UNNotificationActionOptionDestructive;
            } else if (nativeActionType == QStringLiteral("openActivities")) {
                options = UNNotificationActionOptionForeground;
            }
            nativeAction = [UNNotificationAction
                actionWithIdentifier:identifier
                title:label.toNSString()
                options:options];
        }

        auto * const metadata = [NSMutableDictionary dictionaryWithObject:nativeActionType.toNSString()
                                                                    forKey:actionTypeUserInfoKey];
        if (actionIndex >= 0 && actionIndex < activity._links.size()) {
            const auto &link = activity._links.at(actionIndex);
            [metadata setObject:link._link.toNSString() forKey:actionLinkUserInfoKey];
            [metadata setObject:QString::fromUtf8(link._verb).toNSString() forKey:actionVerbUserInfoKey];
        }
        [responseActions setObject:metadata forKey:identifier];
        [nativeActions addObject:nativeAction];

        categorySignature.append(identifier.UTF8String);
        categorySignature.append('\0');
        categorySignature.append(label.toUtf8());
        categorySignature.append('\0');
        categorySignature.append(nativeActionType.toUtf8());
        categorySignature.append('\0');
        categorySignature.append(verb);
        categorySignature.append('\0');
    }

    const auto categoryDigest = QCryptographicHash::hash(categorySignature, QCryptographicHash::Sha256).toHex();
    NSString * const categoryIdentifier = QStringLiteral("%1%2")
                                              .arg(QString::fromNSString(serverCategoryPrefix), QString::fromLatin1(categoryDigest))
                                              .toNSString();
    UNNotificationCategory * const category = [UNNotificationCategory
        categoryWithIdentifier:categoryIdentifier
        actions:nativeActions
        intentIdentifiers:@[]
        options:UNNotificationCategoryOptionCustomDismissAction];
    registerNativeNotificationCategory(category);

    auto * const content = basicNotificationContent(previewText.title, message);
    content.categoryIdentifier = categoryIdentifier;

    const auto accountId = accountState->account()->id();
    auto * const userInfo = [NSMutableDictionary dictionaryWithObjectsAndKeys:
        accountId.toNSString(), accountIdUserInfoKey,
        [NSNumber numberWithLongLong:activity._id], notificationIdUserInfoKey,
        responseActions, responseActionsUserInfoKey,
        nil];
    if (!activity._talkNotificationData.conversationToken.isEmpty()) {
        [userInfo setObject:activity._talkNotificationData.conversationToken.toNSString() forKey:@"token"];
    }
    if (!activity._talkNotificationData.messageId.isEmpty()) {
        [userInfo setObject:activity._talkNotificationData.messageId.toNSString() forKey:@"replyTo"];
    }
    content.userInfo = userInfo;

    const auto requestIdentifier = nativeServerNotificationIdentifier(accountId, activity._id);
    UNTimeIntervalNotificationTrigger * const trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1 repeats:NO];
    UNNotificationRequest * const request = [UNNotificationRequest requestWithIdentifier:requestIdentifier.toNSString()
                                                                                  content:content
                                                                                  trigger:trigger];
    [center addNotificationRequest:request withCompletionHandler:^(NSError *error) {
        if (error) {
            qCWarning(lcMacNotificationCenter) << "Could not deliver server notification:"
                                                     << QString::fromNSString(error.localizedDescription);
        }
    }];
}

void removeServerNotification(const QString &accountId, const qint64 notificationId)
{
    const auto identifier = nativeServerNotificationIdentifier(accountId, notificationId).toNSString();
    auto * const center = UNUserNotificationCenter.currentNotificationCenter;
    [center removePendingNotificationRequestsWithIdentifiers:@[identifier]];
    [center removeDeliveredNotificationsWithIdentifiers:@[identifier]];
}

void reconcileServerNotifications(const QString &accountId, const QSet<qint64> &activeNotificationIds)
{
    auto * const center = UNUserNotificationCenter.currentNotificationCenter;
    const auto capturedAccountId = accountId;
    const auto capturedActiveNotificationIds = activeNotificationIds;
    __block quint64 generation = 0;
    performOnMainThread(^{
        generation = beginServerNotificationReconciliation(capturedAccountId);
    });

    [center getPendingNotificationRequestsWithCompletionHandler:^(NSArray<UNNotificationRequest *> *requests) {
        performOnMainThread(^{
            if (!isCurrentServerNotificationReconciliation(capturedAccountId, generation)) {
                return;
            }
            removePendingAndDeliveredNotifications(staleServerNotificationIdentifiers(requests, capturedAccountId, capturedActiveNotificationIds));
        });
    }];

    [center getDeliveredNotificationsWithCompletionHandler:^(NSArray<UNNotification *> *notifications) {
        performOnMainThread(^{
            if (!isCurrentServerNotificationReconciliation(capturedAccountId, generation)) {
                return;
            }
            auto * const requests = [NSMutableArray arrayWithCapacity:notifications.count];
            for (UNNotification *notification in notifications) {
                [requests addObject:notification.request];
            }
            removePendingAndDeliveredNotifications(staleServerNotificationIdentifiers(requests, capturedAccountId, capturedActiveNotificationIds));
        });
    }];
}

} // namespace OCC::MacNotificationCenter

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncnotificationcenterdelegate.h"

#include "macnotificationcenter_p.h"

#include <QtGlobal>

@implementation NCNotificationCenterDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler
{
    Q_UNUSED(center);
    Q_UNUSED(notification);
    OCC::MacNotificationCenter::willPresentNotification(completionHandler);
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
didReceiveNotificationResponse:(UNNotificationResponse *)response
         withCompletionHandler:(void (^)(void))completionHandler
{
    Q_UNUSED(center);
    OCC::MacNotificationCenter::didReceiveNotificationResponse(response, completionHandler);
}

@end

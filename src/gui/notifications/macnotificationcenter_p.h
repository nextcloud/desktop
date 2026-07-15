/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <UserNotifications/UserNotifications.h>

namespace OCC::MacNotificationCenter {

void willPresentNotification(void (^completionHandler)(UNNotificationPresentationOptions options));
void didReceiveNotificationResponse(UNNotificationResponse *response, void (^completionHandler)(void));

}

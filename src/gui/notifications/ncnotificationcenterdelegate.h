/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <UserNotifications/UserNotifications.h>

/** @brief Forwards macOS notification center callbacks to the C++ notification module. */
@interface NCNotificationCenterDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

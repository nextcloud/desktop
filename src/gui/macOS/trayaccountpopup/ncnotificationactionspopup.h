/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

#include <QVariantList>

@class NCTrayPopup;

/**
 * @brief A borderless popup listing the actions available for a single notification.
 */
@interface NCNotificationActionsPopup : NSPanel
/**
 * @brief Rebuilds the popup for a notification's actions.
 * @param activityIndex Index of the notification's activity in the UserModel.
 * @param actions The notification's actions as provided by the UserModel.
 * @param owner The root tray popup, used to open the Activities window or close all popups.
 */
- (void)populateForUserIndex:(int)userIndex activityIndex:(int)activityIndex actions:(QVariantList)actions owner:(NCTrayPopup *)owner;
@end

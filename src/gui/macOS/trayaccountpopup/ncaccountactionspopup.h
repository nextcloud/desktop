/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

#include <QVariantList>

@class NCTrayPopup;

/**
 * @brief The submenu shown for a single account.
 *
 * Lists the user status, "Reveal in Finder", the Assistant and Apps shortcuts,
 * pending notifications and recent activity. Owns the apps and
 * notification-actions sub-popups.
 */
@interface NCAccountActionsPopup : NSPanel
/** @brief Rebuilds the popup for the given account and refreshes its activity preview. */
- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner;
/**
 * @brief Rebuilds the popup for the given account.
 * @param refreshActivities When NO, the activity preview is not re-fetched
 *        (used to rebuild in place when the model reports new data).
 */
- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner refreshActivities:(BOOL)refreshActivities;
/** @brief Whether the popup is currently visible and showing the given account. */
- (BOOL)isShowingActivitiesForUserIndex:(int)userIndex;
/** @brief Removes the persistent highlight from the row whose submenu is currently open. */
- (void)clearActiveSubmenuRow;
/** @brief Hides the apps and notification-actions sub-popups and clears the active submenu row. */
- (void)hideAppsPopup;
/** @brief Shows the notification-actions sub-popup anchored to the given @c row. */
- (void)showNotificationActionsPopupFromRow:(NSView *)row forUserIndex:(int)userIndex activityIndex:(int)activityIndex actions:(QVariantList)actions;
@end

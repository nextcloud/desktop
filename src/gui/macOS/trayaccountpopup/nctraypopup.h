/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

#import "ncaccountrow.h"

/**
 * @brief The root tray popup.
 *
 * A borderless panel listing the configured accounts plus the global
 * "Add account", "Settings" and "Quit" actions. Acts as the delegate for its
 * account rows and owns the per-account actions submenu.
 */
@interface NCTrayPopup : NSPanel <NCAccountRowDelegate>
/** @brief Rebuilds the account list and global action rows and resizes the panel to fit. */
- (void)populate;
/** @brief Closes this popup and the account-actions submenu and marks the systray closed. */
- (void)closeAllPopups;
/** @brief Closes only the account-actions submenu and clears the active account row. */
- (void)closeAccountActionsPopup;
/** @brief Removes the persistent highlight from the currently active account row. */
- (void)clearActiveAccountRow;
/** @brief Closes the popups and opens the Activities window for the given account. */
- (void)openActivitiesForIndex:(int)index;
/** @brief Closes the popups and reveals the given account's local folder (or file provider domain) in Finder. */
- (void)openLocalFolderForIndex:(int)index;
/** @brief Closes the popups and opens the Assistant window for the given account. */
- (void)openAssistantForIndex:(int)index;
/** @brief Closes the popups and opens the user-status window for the given account. */
- (void)openOnlineStatusForIndex:(int)index;
@end

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FinderSyncProtocol_h
#define FinderSyncProtocol_h

#import <Foundation/Foundation.h>

/**
 * @brief The FinderSync extension APIs exposed through XPC.
 *
 * This protocol is implemented by the FinderSync extension and allows the main app
 * to communicate with the extension via XPC instead of UNIX sockets.
 */
@protocol FinderSyncProtocol

/**
 * @brief Register a path for sync monitoring.
 * @param path The absolute path to register for monitoring.
 */
- (void)registerPath:(NSString *)path;

/**
 * @brief Unregister a path from sync monitoring.
 * @param path The absolute path to unregister.
 */
- (void)unregisterPath:(NSString *)path;

/**
 * @brief Notify the extension to update the view for a given path.
 * @param path The absolute path where the view should be refreshed.
 */
- (void)updateViewAtPath:(NSString *)path;

/**
 * @brief Set the sync status result for a specific file or folder.
 * @param status The status string (e.g., "SYNC", "OK", "ERROR", "OK+SWM").
 * @param path The absolute path of the file or folder.
 */
- (void)setStatusResult:(NSString *)status forPath:(NSString *)path;

/**
 * @brief Set a localized string value for a given key.
 * @param value The localized string value.
 * @param key The string key (e.g., "CONTEXT_MENU_TITLE").
 */
- (void)setLocalizedString:(NSString *)value forKey:(NSString *)key;

/**
 * @brief Reset all menu items (clears the current menu item list).
 */
- (void)resetMenuItems;

/**
 * @brief Add a menu item to the context menu.
 * @param command The command identifier (e.g., "SHARE", "LOCK_FILE").
 * @param flags The menu item flags (empty string for enabled, "d" for disabled).
 * @param text The menu item display text.
 */
- (void)addMenuItemWithCommand:(NSString *)command
                         flags:(NSString *)flags
                          text:(NSString *)text;

/**
 * @brief Signal that all menu items have been sent (marks menu as complete).
 */
- (void)menuItemsComplete;

@end

#endif /* FinderSyncProtocol_h */

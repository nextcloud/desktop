/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FinderSyncAppProtocol_h
#define FinderSyncAppProtocol_h

#import <Foundation/Foundation.h>

/**
 * @brief The main app APIs exposed through XPC.
 *
 * This protocol is implemented by the main Nextcloud app and allows the FinderSync
 * extension to query file statuses, menu items, and execute commands via XPC.
 */
@protocol FinderSyncAppProtocol

/**
 * @brief Retrieve the sync status for a file.
 * @param path The absolute path to the file.
 * @param completionHandler Callback with status string (e.g., "SYNC", "OK", "ERROR") or error.
 */
- (void)retrieveFileStatusForPath:(NSString *)path
                completionHandler:(void(^)(NSString *status, NSError *error))completionHandler;

/**
 * @brief Retrieve the sync status for a folder.
 * @param path The absolute path to the folder.
 * @param completionHandler Callback with status string (e.g., "SYNC", "OK", "ERROR") or error.
 */
- (void)retrieveFolderStatusForPath:(NSString *)path
                  completionHandler:(void(^)(NSString *status, NSError *error))completionHandler;

/**
 * @brief Get localized strings for the FinderSync extension UI.
 * @param completionHandler Callback with dictionary of string keys and localized values, or error.
 */
- (void)getLocalizedStringsWithCompletionHandler:(void(^)(NSDictionary<NSString *, NSString *> *strings, NSError *error))completionHandler;

/**
 * @brief Get context menu items for the given paths.
 * @param paths Array of absolute paths for which to get menu items.
 * @param completionHandler Callback with array of menu item dictionaries (command, flags, text) or error.
 */
- (void)getMenuItemsForPaths:(NSArray<NSString *> *)paths
           completionHandler:(void(^)(NSArray<NSDictionary *> *menuItems, NSError *error))completionHandler;

/**
 * @brief Execute a menu command for the given paths.
 * @param command The command identifier (e.g., "SHARE", "ACTIVITY").
 * @param paths Array of absolute paths on which to execute the command.
 * @param completionHandler Callback with error if execution failed, or nil on success.
 */
- (void)executeMenuCommand:(NSString *)command
                  forPaths:(NSArray<NSString *> *)paths
         completionHandler:(void(^)(NSError *error))completionHandler;

@end

#endif /* FinderSyncAppProtocol_h */

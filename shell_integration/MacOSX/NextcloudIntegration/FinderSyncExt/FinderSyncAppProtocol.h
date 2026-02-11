/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FinderSyncAppProtocol_h
#define FinderSyncAppProtocol_h

#import <Foundation/Foundation.h>

/**
 * @brief The main app APIs exposed through XPC for FinderSync extension.
 *
 * This protocol is implemented by the main application and allows the FinderSync
 * extension to request information and execute commands via XPC.
 */
@protocol FinderSyncAppProtocol

/**
 * @brief Retrieve the sync status for a specific file.
 * @param path The absolute path of the file.
 * @param completionHandler Called with the status string (e.g., "SYNC", "OK", "ERROR") or error.
 */
- (void)retrieveFileStatusForPath:(NSString *)path
                completionHandler:(void(^)(NSString *status, NSError *error))completionHandler;

/**
 * @brief Retrieve the sync status for a specific folder.
 * @param path The absolute path of the folder.
 * @param completionHandler Called with the status string or error.
 */
- (void)retrieveFolderStatusForPath:(NSString *)path
                  completionHandler:(void(^)(NSString *status, NSError *error))completionHandler;

/**
 * @brief Get all localized strings for the FinderSync extension UI.
 * @param completionHandler Called with a dictionary of key-value string pairs or error.
 */
- (void)getLocalizedStringsWithCompletionHandler:(void(^)(NSDictionary<NSString *, NSString *> *strings, NSError *error))completionHandler;

/**
 * @brief Get context menu items for the specified file/folder paths.
 * @param paths Array of absolute paths for which to generate menu items.
 * @param completionHandler Called with an array of menu item dictionaries (command, flags, text) or error.
 */
- (void)getMenuItemsForPaths:(NSArray<NSString *> *)paths
           completionHandler:(void(^)(NSArray<NSDictionary *> *menuItems, NSError *error))completionHandler;

/**
 * @brief Execute a menu command for the specified paths.
 * @param command The command identifier (e.g., "SHARE", "LOCK_FILE", "ACTIVITY").
 * @param paths Array of absolute paths to which the command applies.
 * @param completionHandler Called when the command completes, with error if any.
 */
- (void)executeMenuCommand:(NSString *)command
                  forPaths:(NSArray<NSString *> *)paths
         completionHandler:(void(^)(NSError *error))completionHandler;

@end

#endif /* FinderSyncAppProtocol_h */

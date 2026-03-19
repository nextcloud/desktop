/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <Foundation/Foundation.h>
#import "SyncClient.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * @brief Manages XPC communication between FinderSync extension and the main app.
 *
 * This class replaces the LocalSocketClient for FinderSync, providing XPC-based
 * communication instead of UNIX sockets. It establishes a connection to the main app's
 * XPC service and handles method calls in both directions.
 */
@interface FinderSyncXPCManager : NSObject

/**
 * @brief The delegate that receives callbacks from the main app via XPC.
 */
@property (nonatomic, weak) id<SyncClientDelegate> delegate;

/**
 * @brief Initialize the XPC manager.
 * @param delegate The delegate to receive XPC callbacks.
 */
- (instancetype)initWithDelegate:(id<SyncClientDelegate>)delegate;

/**
 * @brief Start the XPC connection to the main app.
 */
- (void)start;

/**
 * @brief Check if the XPC connection is established.
 * @return YES if connected, NO otherwise.
 */
- (BOOL)isConnected;

/**
 * @brief Request the badge icon for a file or directory.
 * @param path The absolute path of the file/directory.
 * @param isDirectory YES if the path is a directory, NO if it's a file.
 */
- (void)askForIcon:(NSString *)path isDirectory:(BOOL)isDirectory;

/**
 * @brief Send a query to the main app (generic method for menu commands, etc.).
 * @param arg The argument string (usually file paths separated by record separator).
 * @param query The command/query string (e.g., "GET_MENU_ITEMS", "SHARE", "LOCK_FILE").
 */
- (void)askOnSocket:(NSString *)arg query:(NSString *)query;

@end

NS_ASSUME_NONNULL_END

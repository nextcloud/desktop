/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AppProtocol_h
#define AppProtocol_h

#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN

/**
 * @brief The main app APIs exposed through XPC.
 */
@protocol AppProtocol

/**
 * @brief The file provider extension can tell the main app to offer the user server-features for the given item.
 * @param fileId The ocId as provided by the server for item identification independent from path.
 * @param path The local and absolute path for the item to offer actions for.
 * @param remoteItemPath The server-side path of the item, used as a fallback when no sync folder is configured.
 * @param domainIdentifier The file provider domain identifier for the account that manages this file.
 */
- (void)presentFileActions:(NSString *)fileId path:(NSString *)path remoteItemPath:(NSString *)remoteItemPath withDomainIdentifier:(NSString *)domainIdentifier;

/**
 * @brief The file provider extension can report its synchronization status as a string constant value to the main app through this method.
 * @param status The synchronization status string.
 * @param domainIdentifier The file provider domain identifier for which the status is reported.
 */
- (void)reportSyncStatus:(NSString *)status forDomainIdentifier:(NSString *)domainIdentifier;

/**
 * @brief The file provider extension reports an item it refused to sync because that kind of item isn't supported yet (currently: macOS bundles).
 *
 * The main app surfaces the message in its activity view in the systray's tray window — the same place the classic sync engine reports excluded items.
 *
 * @param relativePath The path of the item relative to the file provider domain root.
 * @param fileName The display name of the item.
 * @param reason A localized, human-readable explanation of why the item was excluded. Already translated extension-side.
 * @param domainIdentifier The file provider domain identifier for the account that owns the item.
 */
- (void)reportItemExcludedFromSync:(NSString *)relativePath
                          fileName:(NSString *)fileName
                            reason:(NSString *)reason
               forDomainIdentifier:(NSString *)domainIdentifier;

@end

NS_ASSUME_NONNULL_END
#endif /* AppProtocol_h */


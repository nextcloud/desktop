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

@end

NS_ASSUME_NONNULL_END
#endif /* AppProtocol_h */


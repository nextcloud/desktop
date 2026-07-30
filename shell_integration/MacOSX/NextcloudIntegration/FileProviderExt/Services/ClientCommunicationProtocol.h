/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ClientCommunicationProtocol_h
#define ClientCommunicationProtocol_h

#import <Foundation/Foundation.h>

@protocol ClientCommunicationProtocol

/**
 * @brief Get the raw file provider domain identifier value.
 */
- (void)getFileProviderDomainIdentifierWithCompletionHandler:(void(^)(NSString *extensionAccountId, NSError *error))completionHandler;

/**
 * @brief Ask the file provider extension whether it has dirty user data.
 */
- (void)hasDirtyUserDataWithCompletionHandler:(void(^)(BOOL hasDirtyUserData))completionHandler;

- (void)configureAccountWithUser:(NSString *)user
                          userId:(NSString *)userId
                       serverUrl:(NSString *)serverUrl
                        password:(NSString *)password
                       userAgent:(NSString *)userAgent;
- (void)removeAccountConfig;
- (void)setIgnoreList:(NSArray<NSString *> *)ignoreList;

/**
 * @brief Process numeric WebDAV file IDs received from notify-push.
 *
 * An empty array represents a legacy notification without file IDs and must trigger a refresh.
 * Otherwise, the extension refreshes only if at least one file ID belongs to locally known metadata.
 */
- (void)processFileIdsChanged:(NSArray<NSNumber *> *)fileIds
             completionHandler:(void(^)(BOOL processed))completionHandler;

@end

#endif /* ClientCommunicationProtocol_h */

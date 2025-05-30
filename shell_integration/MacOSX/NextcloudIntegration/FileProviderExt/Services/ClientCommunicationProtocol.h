/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ClientCommunicationProtocol_h
#define ClientCommunicationProtocol_h

#import <Foundation/Foundation.h>

@protocol ClientCommunicationProtocol

- (void)getExtensionAccountIdWithCompletionHandler:(void(^)(NSString *extensionAccountId, NSError *error))completionHandler;
- (void)configureAccountWithUser:(NSString *)user
                          userId:(NSString *)userId
                       serverUrl:(NSString *)serverUrl
                        password:(NSString *)password
                       userAgent:(NSString *)userAgent;
- (void)removeAccountConfig;
- (void)createDebugLogStringWithCompletionHandler:(void(^)(NSString *debugLogString, NSError *error))completionHandler;
- (void)getTrashDeletionEnabledStateWithCompletionHandler:(void(^)(BOOL enabled, BOOL set))completionHandler;
- (void)setTrashDeletionEnabled:(BOOL)enabled;
- (void)setIgnoreList:(NSArray<NSString *> *)ignoreList;

@end

#endif /* ClientCommunicationProtocol_h */

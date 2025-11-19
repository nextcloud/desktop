/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <Foundation/Foundation.h>

@protocol SyncClientDelegate <NSObject>
- (void)setResult:(NSString *)result forPath:(NSString *)path;
- (void)reFetchFileNameCacheForPath:(NSString *)path;
- (void)registerPath:(NSString *)path;
- (void)unregisterPath:(NSString *)path;
- (void)setString:(NSString *)key value:(NSString *)value;
- (void)resetMenuItems;
- (void)addMenuItem:(NSDictionary *)item;
- (void)menuHasCompleted;
- (void)connectionDidDie;
@end

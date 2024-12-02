/*
 * Copyright (C) by Jocelyn Turcotte <jturcotte@woboq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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

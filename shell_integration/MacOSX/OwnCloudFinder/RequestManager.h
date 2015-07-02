/**
 * Copyright (c) 2000-2012 Liferay, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#import <Foundation/Foundation.h>
#import "RequestManager.h"
#import "SyncClientProxy.h"

@interface OwnCloudFinderRequestManager : NSObject <SyncClientProxyDelegate>
{
	SyncClientProxy *_syncClientProxy;

	NSMutableArray* _requestQueue;
	NSMutableDictionary* _registeredPathes;
	NSMutableSet* _requestedPaths;

	NSString *_shareMenuTitle;
}

@property (nonatomic, retain) NSString* filterFolder;

+ (OwnCloudFinderRequestManager*)sharedInstance;

- (BOOL)isRegisteredPath:(NSString*)path isDirectory:(BOOL)isDir;
- (void)askForIcon:(NSString*)path isDirectory:(BOOL)isDir;
- (void)menuItemClicked:(NSDictionary*)actionDictionary;

- (NSString*) shareItemTitle;

@end
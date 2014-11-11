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
#import "GCDAsyncSocket.h"
#import "RequestManager.h"

@interface RequestManager : NSObject
{
	GCDAsyncSocket* _socket;

	NSMutableArray* _requestQueue;
	NSMutableDictionary* _registeredPathes;
	
	BOOL _isRunning;
	BOOL _isConnected;
}

@property (nonatomic, retain) NSString* filterFolder;

+ (RequestManager*)sharedInstance;

- (BOOL)isRegisteredPath:(NSString*)path isDirectory:(BOOL)isDir;
- (void)askOnSocket:(NSString*)path query:(NSString*)verb;
- (NSNumber*)askForIcon:(NSString*)path isDirectory:(BOOL)isDir;
- (void)menuItemClicked:(NSDictionary*)actionDictionary;
- (void)start;

@end
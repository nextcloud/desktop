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

#import "ContentManager.h"
#import "IconCache.h"
#import "RequestManager.h"

static OwnCloudFinderRequestManager* sharedInstance = nil;

@implementation OwnCloudFinderRequestManager

- (id)init
{
	if ((self = [super init]))
	{
		// For the sake of allowing both the legacy and the FinderSync extensions to work with the same
		// client build, use the same server name including the Team ID even though we won't be sandboxed.
		NSBundle *extBundle = [NSBundle bundleForClass:[self class]];
		// This was added to the bundle's Info.plist to get it from the build system
		NSString *socketApiPrefix = [extBundle objectForInfoDictionaryKey:@"SocketApiPrefix"];
		NSString *serverName = [socketApiPrefix stringByAppendingString:@".socketApi"];
		// NSLog(@"OwnCloudFinderRequestManager serverName %@", serverName);

		_syncClientProxy = [[SyncClientProxy alloc] initWithDelegate:self serverName:serverName];

		_registeredPathes = [[NSMutableDictionary alloc] init];
		_requestedPaths = [[NSMutableSet alloc] init];

		_shareMenuTitle = nil;

		// The NSConnection will block until the distant object came back and this creates a loop hanging Finder.
		// Start from a timer to have time to unwind the stack first.
		[NSTimer scheduledTimerWithTimeInterval:0 target:_syncClientProxy selector:@selector(start) userInfo:nil repeats:NO];
	}

	return self;
}

- (void)dealloc
{
	[_syncClientProxy release];

	sharedInstance = nil;

	[super dealloc];
}

+ (OwnCloudFinderRequestManager*)sharedInstance
{
	@synchronized(self)
	{
		if (sharedInstance == nil)
		{
			sharedInstance = [[self alloc] init];
		}
	}

	return sharedInstance;
}

- (BOOL)isRegisteredPath:(NSString*)path isDirectory:(BOOL)isDir
{
	// check if the file in question is underneath a registered directory
	NSArray *regPathes = [_registeredPathes allKeys];
	BOOL registered = NO;

	NSString* checkPath = [NSString stringWithString:path];
	if (isDir && ![checkPath hasSuffix:@"/"]) {
		// append a slash
		checkPath = [path stringByAppendingString:@"/"];
	}

	for( NSString *regPath in regPathes ) {
		if( [checkPath hasPrefix:regPath]) {
			// the path was registered
			registered = YES;
			break;
		}
	}

	return registered;
}

- (void)askForIcon:(NSString*)path isDirectory:(BOOL)isDir
{
	if( [self isRegisteredPath:path isDirectory:isDir] ) {
		[_requestedPaths addObject:path];
		[_syncClientProxy askForIcon:path isDirectory:isDir];
	}
}

- (void)setResultForPath:(NSString*)path result:(NSString*)result
{
	// The client will broadcast all changes, do not fill the cache for paths that Finder didn't ask for.
	if ([_requestedPaths containsObject:path]) {
		[[OwnCloudFinderContentManager sharedInstance] setResultForPath:path result:result];
	}
}

- (void)reFetchFileNameCacheForPath:(NSString*)path
{
	[_requestedPaths removeAllObjects];
	[[OwnCloudFinderContentManager sharedInstance] reFetchFileNameCacheForPath:path];
}

- (void)registerPath:(NSString*)path
{
	NSNumber *one = [NSNumber numberWithInt:1];
	[_registeredPathes setObject:one forKey:path];
	[[OwnCloudFinderContentManager sharedInstance] repaintAllWindows];
}

- (void)unregisterPath:(NSString*)path
{
	[_registeredPathes removeObjectForKey:path];
	[[OwnCloudFinderContentManager sharedInstance] repaintAllWindows];
}

- (void)setShareMenuTitle:(NSString*)title
{
	_shareMenuTitle = title;
}

- (void)connectionDidDie
{
	// NSLog(@"Socket DISconnected! %@", [err localizedDescription]);

	// clear the registered pathes.
	[_registeredPathes release];
	_registeredPathes = [[NSMutableDictionary alloc] init];
	[_requestedPaths removeAllObjects];

    // clear the caches in conent manager
	OwnCloudFinderContentManager *contentman = [OwnCloudFinderContentManager sharedInstance];
	[contentman clearFileNameCache];
	[contentman repaintAllWindows];
}

- (void)menuItemClicked:(NSDictionary*)actionDictionary
{
	// NSLog(@"RequestManager menuItemClicked %@", actionDictionary);
	NSArray *filePaths = [actionDictionary valueForKey:@"files"];
	for (int i = 0; i < filePaths.count; i++) {
		[_syncClientProxy askOnSocket:[filePaths objectAtIndex:i] query:@"SHARE"];
	}
}

- (NSString*) shareItemTitle
{
	return _shareMenuTitle;
}

@end

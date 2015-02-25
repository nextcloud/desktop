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

#define READ_TAG 2422

static RequestManager* sharedInstance = nil;

@implementation RequestManager

- (id)init
{
	if ((self = [super init]))
	{
		_socket = [[GCDAsyncSocket alloc] initWithDelegate:self delegateQueue:dispatch_get_main_queue()];

		_isConnected = NO;

		_registeredPathes = [[NSMutableDictionary alloc] init];

		_shareMenuTitle = nil;

		[NSTimer scheduledTimerWithTimeInterval:5 target:self selector:@selector(start) userInfo:nil repeats:YES];
	}

	return self;
}

- (void)dealloc
{
	[_socket setDelegate:nil delegateQueue:NULL];
	[_socket disconnect];
	[_socket release];

	sharedInstance = nil;

	[super dealloc];
}

+ (RequestManager*)sharedInstance
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


- (void)askOnSocket:(NSString*)path query:(NSString*)verb
{
	NSString *query = [NSString stringWithFormat:@"%@:%@\n", verb,path];
	// NSLog(@"Query: %@", query);
	
	NSData* data = [query dataUsingEncoding:NSUTF8StringEncoding];
	[_socket writeData:data withTimeout:5 tag:4711];
	
	NSData* stop = [@"\n" dataUsingEncoding:NSUTF8StringEncoding];
	[_socket readDataToData:stop withTimeout:-1 tag:READ_TAG];
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

- (NSNumber*)askForIcon:(NSString*)path isDirectory:(BOOL)isDir
{
	NSString *verb = @"RETRIEVE_FILE_STATUS";
	NSNumber *res = [NSNumber numberWithInt:0];

	if( [self isRegisteredPath:path isDirectory:isDir] ) {
		if( _isConnected ) {
			if(isDir) {
				verb = @"RETRIEVE_FOLDER_STATUS";
			}

			[self askOnSocket:path query:verb];

			NSNumber *res_minus_one = [NSNumber numberWithInt:0];

			return res_minus_one;
		} else {
			[_requestQueue addObject:path];
			[self start]; // try again to connect
		}
	}
	return res;
}


- (void)socket:(GCDAsyncSocket*)socket didReadData:(NSData*)data withTag:(long)tag
{
	NSString *answer = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
	NSArray *chunks = nil;
	if (answer != nil && [answer length] > 0) {
		// cut a trailing newline
		answer = [answer substringToIndex:[answer length] - 1];
		chunks = [answer componentsSeparatedByString: @":"];
	}
	ContentManager *contentman = [ContentManager sharedInstance];

	if( chunks && [chunks count] > 0 && tag == READ_TAG ) {
		// NSLog(@"READ from socket (%ld): <%@>", tag, answer);
		if( [[chunks objectAtIndex:0] isEqualToString:@"STATUS"] ) {
			NSString *path = [chunks objectAtIndex:2];
			if( [chunks count] > 3 ) {
				for( int i = 2; i < [chunks count]-1; i++ ) {
					path = [NSString stringWithFormat:@"%@:%@",
							path, [chunks objectAtIndex:i+1] ];
				}
			}
			[contentman setResultForPath:path result:[chunks objectAtIndex:1]];
		} else if( [[chunks objectAtIndex:0] isEqualToString:@"UPDATE_VIEW"] ) {
			NSString *path = [chunks objectAtIndex:1];
			[contentman reFetchFileNameCacheForPath:path];
		} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"REGISTER_PATH"] ) {
			NSNumber *one = [NSNumber numberWithInt:1];
			NSString *path = [chunks objectAtIndex:1];
			// NSLog(@"Registering path: %@", path);
			[_registeredPathes setObject:one forKey:path];
			
			[contentman repaintAllWindows];
		} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"UNREGISTER_PATH"] ) {
			NSString *path = [chunks objectAtIndex:1];
			[_registeredPathes removeObjectForKey:path];

			[contentman repaintAllWindows];
		} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"ICON_PATH"] ) {
			NSString *path = [chunks objectAtIndex:1];
			[[ContentManager sharedInstance] loadIconResourcePath:path];
		} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"SHARE_MENU_TITLE"] ) {
			_shareMenuTitle = [[chunks objectAtIndex:1] copy];
				// NSLog(@"Received shar menu title: %@", _shareMenuTitle);
		} else {
			NSLog(@"SyncState: Unknown command %@", [chunks objectAtIndex:0]);
		}
	} else if (tag != READ_TAG) {
		NSLog(@"SyncState: Received unknown tag %ld <%@>", tag, answer);
	}
	// Read on and on
	NSData* stop = [@"\n" dataUsingEncoding:NSUTF8StringEncoding];
	[_socket readDataToData:stop withTimeout:-1 tag:READ_TAG];

}

- (NSTimeInterval)socket:(GCDAsyncSocket*)socket shouldTimeoutReadWithTag:(long)tag elapsed:(NSTimeInterval)elapsed bytesDone:(NSUInteger)length
{
	// Called if a read operation has reached its timeout without completing.
	return 0.0;
}

-(void)socket:(GCDAsyncSocket*)socket didConnectToUrl:(NSURL *)url {
	// NSLog( @"Connected to sync client successfully on %@", url);
	_isConnected = YES;

	[self askOnSocket:@"" query:@"SHARE_MENU_TITLE"];

	if( [_requestQueue count] > 0 ) {
		// NSLog( @"We have to empty the queue");
		for( NSString *path in _requestQueue ) {
			[self askOnSocket:path query:@"RETRIEVE_FILE_STATUS"];
		}
	}

	ContentManager *contentman = [ContentManager sharedInstance];
	[contentman clearFileNameCacheForPath:nil];
	[contentman repaintAllWindows];

	// Read for the UPDATE_VIEW requests
	NSData* stop = [@"\n" dataUsingEncoding:NSUTF8StringEncoding];
	[_socket readDataToData:stop withTimeout:-1 tag:READ_TAG];
}

- (void)socketDidDisconnect:(GCDAsyncSocket*)socket withError:(NSError*)err
{
	// NSLog(@"Socket DISconnected! %@", [err localizedDescription]);

	_isConnected = NO;

	// clear the registered pathes.
	[_registeredPathes release];
	_registeredPathes = [[NSMutableDictionary alloc] init];

    // clear the caches in conent manager
	ContentManager *contentman = [ContentManager sharedInstance];
	[contentman clearFileNameCacheForPath:nil];
	[contentman repaintAllWindows];
}

- (NSDate*)fileDate:(NSString*)fn
{
	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSError *error = 0;
	NSDictionary *oneAttributes = [fileManager attributesOfItemAtPath:fn error:&error];
	if (!error) {
		return [oneAttributes valueForKey:NSFileModificationDate];
	}
	return nil;
}

- (NSString*) getNewerFileOne:(NSString*)one two:(NSString*)two
{
	if (!one) {
		return two;
	}
	NSDate *oneDate = [self fileDate:one];
	NSDate *twoDate = [self fileDate:two];
	if (oneDate && twoDate) {
		if ([oneDate compare:twoDate] == NSOrderedDescending) {
			return one;
		} else if ([oneDate compare:twoDate] == NSOrderedAscending) {
			return two;
		} else {
			return two;
		}
	}
	return one;
}


- (void)start
{
	if (!_isConnected && ![_socket isConnected])
	{
		NSError *err = nil;
		NSURL *url = nil;
		NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
		if ([paths count])
		{
			// e.g file:///Users/guruz/Library/Caches/SyncStateHelper/ownCloud.socket
			NSString *syncStateHelperDir = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"SyncStateHelper"];
			NSError *pnsError = NULL;
			NSArray *paths = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:syncStateHelperDir error:&pnsError];
			if (!pnsError && paths && [paths count] > 0) {
				NSString *currentLatestPath = nil;
				if (paths.count > 1) {
					// NSLog(@"Possible paths: %@", paths);
				}
				for (int i = 0; i < paths.count; i++) {
					NSString *currentPath = [syncStateHelperDir stringByAppendingPathComponent:[paths objectAtIndex:i]];
					if (![currentPath hasSuffix:@".socket"]) {
						continue;
					}
					currentLatestPath = [self getNewerFileOne:currentLatestPath two:currentPath];
				}
				// FIXME Instead of connecting to the newest socket we could go multi-socket to support multiple instances
				if (currentLatestPath) {
					url = [NSURL fileURLWithPath:currentLatestPath];
				}
			}
		}
		if (url) {
			// NSLog(@"Connect Socket to %@", url);
			[_socket connectToUrl:url withTimeout:1 error:&err];
		}
	}
}

- (void)menuItemClicked:(NSDictionary*)actionDictionary
{
	// NSLog(@"RequestManager menuItemClicked %@", actionDictionary);
	NSArray *filePaths = [actionDictionary valueForKey:@"files"];
	for (int i = 0; i < filePaths.count; i++) {
		[self askOnSocket:[filePaths objectAtIndex:i] query:@"SHARE"];
	}
}

- (NSString*) shareItemTitle
{
	if (_socket && _socket.isConnected && _shareMenuTitle) {
		return _shareMenuTitle;
	}
	return nil;
}



@end

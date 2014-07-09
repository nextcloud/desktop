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
#import "JSONKit.h"
#import "RequestManager.h"

#define READ_TAG 2422

static RequestManager* sharedInstance = nil;

@implementation RequestManager

- (id)init
{
	if ((self = [super init]))
	{
		_socket = [[GCDAsyncSocket alloc] initWithDelegate:self delegateQueue:dispatch_get_main_queue()];
		
		_filterFolder = nil;

		_isRunning = NO;
		_isConnected = NO;

		[self start];
	}

	return self;
}

- (void)dealloc
{
	[_socket setDelegate:nil delegateQueue:NULL];
	[_socket disconnect];
	[_socket release];

	// dispatch_release(_listenQueue);

	// [_callbackMsgs release];

	// [_filterFolder release];

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


- (void)askOnSocket:(NSString*)path
{
	NSLog(@"XX on Socket for %@", path);
	NSData* data = [[NSString stringWithFormat:@"RETRIEVE_FILE_STATUS:%@\n", path] dataUsingEncoding:NSUTF8StringEncoding];
	[_socket writeData:data withTimeout:5 tag:4711];
	
	NSData* stop = [@"\n" dataUsingEncoding:NSUTF8StringEncoding];
	[_socket readDataToData:stop withTimeout:-1 tag:READ_TAG];
}


- (void)askForIcon:(NSString*)path
{
	if( _isConnected ) {
		[self askOnSocket:path];
	} else {
		[_requestQueue addObject:path];
	}
}


- (void)socket:(GCDAsyncSocket*)socket didConnectToHost:(NSString*)host port:(UInt16)port
{
	// [socket readDataToData:[GCDAsyncSocket CRLFData] withTimeout:-1 tag:0];
	
	NSLog( @"Connected to host successfully!");
	_isConnected = YES;
	
	if( [_requestQueue count] > 0 ) {
		NSLog( @"We have to empty the queue");
		for( NSString *path in _requestQueue ) {
			[self askOnSocket:path];
		}
	}
}


- (void)socket:(GCDAsyncSocket*)socket didReadData:(NSData*)data withTag:(long)tag
{
	
	if( tag == READ_TAG) {
	NSString* a1 = [NSString stringWithUTF8String:[data bytes]];
	
	NSString *answer = [a1 stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]];
    NSLog(@"READ from socket (%ld): AA%@OO", tag, answer);
	
	if( answer != nil ) {
		NSArray *chunks = [answer componentsSeparatedByString: @":"];
		// FIXME: Check if chunks[0] equals "STATUS"
		if( [chunks count] > 0 && [[chunks objectAtIndex:0] isEqualToString:@"STATUS"] ) {
			ContentManager *contentman = [ContentManager sharedInstance];
			[contentman setResultForPath:[chunks objectAtIndex:2] result:[chunks objectAtIndex:1]];
		}
	}
	}
}

- (NSTimeInterval)socket:(GCDAsyncSocket*)socket shouldTimeoutReadWithTag:(long)tag elapsed:(NSTimeInterval)elapsed bytesDone:(NSUInteger)length
{
	// Called if a read operation has reached its timeout without completing.
	return 0.0;
}

- (void)socketDidDisconnect:(GCDAsyncSocket*)socket withError:(NSError*)err
{
	if ([_connectedListenSockets containsObject:socket])
	{
		[_connectedListenSockets removeObject:socket];

		[[ContentManager sharedInstance] enableFileIcons:false];
	}

	if ([_connectedCallbackSockets containsObject:socket])
	{
		[_connectedCallbackSockets removeObject:socket];
	}
}


- (void)start
{
	if (!_isRunning)
	{
		NSLog(@"Connect Socket!");
		NSError *err = nil;
		if (![_socket connectToHost:@"localhost" onPort:33001 error:&err]) // Asynchronous!
		{
			// If there was an error, it's likely something like "already connected" or "no delegate set"
			NSLog(@"I goofed: %@", err);
		}
		NSLog(@"Socket Connected!");
		
		 _isRunning = YES;
	}
}

@end

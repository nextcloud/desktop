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


#import "FinderSync.h"


@implementation FinderSync

- (instancetype)init
{
	self = [super init];

	FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
	NSBundle *extBundle = [NSBundle bundleForClass:[self class]];
	// This was added to the bundle's Info.plist to get it from the build system
	NSString *socketApiPrefix = [extBundle objectForInfoDictionaryKey:@"SocketApiPrefix"];

	NSImage *ok = [extBundle imageForResource:@"ok.icns"];
	NSImage *ok_swm = [extBundle imageForResource:@"ok_swm.icns"];
	NSImage *sync = [extBundle imageForResource:@"sync.icns"];
	NSImage *warning = [extBundle imageForResource:@"warning.icns"];
	NSImage *error = [extBundle imageForResource:@"error.icns"];

	[syncController setBadgeImage:ok label:@"Up to date" forBadgeIdentifier:@"OK"];
	[syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"SYNC"];
	[syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"NEW"];
	[syncController setBadgeImage:warning label:@"Ignored" forBadgeIdentifier:@"IGNORE"];
	[syncController setBadgeImage:error label:@"Error" forBadgeIdentifier:@"ERROR"];
	[syncController setBadgeImage:ok_swm label:@"Shared" forBadgeIdentifier:@"OK+SWM"];
	[syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"SYNC+SWM"];
	[syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"NEW+SWM"];
	[syncController setBadgeImage:warning label:@"Ignored" forBadgeIdentifier:@"IGNORE+SWM"];
	[syncController setBadgeImage:error label:@"Error" forBadgeIdentifier:@"ERROR+SWM"];
	
	// The Mach port name needs to:
	// - Be prefixed with the code signing Team ID
	// - Then infixed with the sandbox App Group
	// - The App Group itself must be a prefix of (or equal to) the application bundle identifier
	// We end up in the official signed client with: 9B5WD74GWJ.com.owncloud.desktopclient.socketApi
	// With ad-hoc signing (the '-' signing identity) we must drop the Team ID.
	// When the code isn't sandboxed (e.g. the OC client or the legacy overlay icon extension)
	// the OS doesn't seem to put any restriction on the port name, so we just follow what
	// the sandboxed App Extension needs.
	// https://developer.apple.com/library/mac/documentation/Security/Conceptual/AppSandboxDesignGuide/AppSandboxInDepth/AppSandboxInDepth.html#//apple_ref/doc/uid/TP40011183-CH3-SW24
	NSString *serverName = [socketApiPrefix stringByAppendingString:@".socketApi"];
	// NSLog(@"FinderSync serverName %@", serverName);

	_syncClientProxy = [[SyncClientProxy alloc] initWithDelegate:self serverName:serverName];
	_registeredDirectories = [[NSMutableSet alloc] init];
	_shareMenuTitle = nil;
	
	[_syncClientProxy start];
	return self;
}

#pragma mark - Primary Finder Sync protocol methods

- (void)requestBadgeIdentifierForURL:(NSURL *)url
{
	BOOL isDir;
	if ([[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory: &isDir] == NO) {
		NSLog(@"ERROR: Could not determine file type of %@", [url path]);
		isDir = NO;
	}
	
	NSString* normalizedPath = [[url path] decomposedStringWithCanonicalMapping];
	[_syncClientProxy askForIcon:normalizedPath isDirectory:isDir];
}

#pragma mark - Menu and toolbar item support

- (NSMenu *)menuForMenuKind:(FIMenuKind)whichMenu
{
	FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
	NSMutableSet *rootPaths = [[NSMutableSet alloc] init];
	[syncController.directoryURLs enumerateObjectsUsingBlock: ^(id obj, BOOL *stop) {
		[rootPaths addObject:[obj path]];
	}];

	// The server doesn't support sharing a root directory so do not show the option in this case.
	// It is still possible to get a problematic sharing by selecting both the root and a child,
	// but this is so complicated to do and meaningless that it's not worth putting this check
	// also in shareMenuAction.
	__block BOOL onlyRootsSelected = YES;
	[syncController.selectedItemURLs enumerateObjectsUsingBlock: ^(id obj, NSUInteger idx, BOOL *stop) {
		if (![rootPaths member:[obj path]]) {
			onlyRootsSelected = NO;
			*stop = YES;
		}
	}];

	if (_shareMenuTitle && !onlyRootsSelected) {
		NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
		NSMenuItem *item = [menu addItemWithTitle:_shareMenuTitle action:@selector(shareMenuAction:) keyEquivalent:@"title"];
		item.image = [[NSBundle mainBundle] imageForResource:@"app.icns"];
		
		return menu;
	}
	return nil;
}

- (IBAction)shareMenuAction:(id)sender
{
	NSArray* items = [[FIFinderSyncController defaultController] selectedItemURLs];
	
	[items enumerateObjectsUsingBlock: ^(id obj, NSUInteger idx, BOOL *stop) {
		NSString* normalizedPath = [[obj path] decomposedStringWithCanonicalMapping];
		[_syncClientProxy askOnSocket:normalizedPath query:@"SHARE"];
	}];
}

#pragma mark - SyncClientProxyDelegate implementation

- (void)setResultForPath:(NSString*)path result:(NSString*)result
{
	NSString *normalizedPath = [path decomposedStringWithCanonicalMapping];
	[[FIFinderSyncController defaultController] setBadgeIdentifier:result forURL:[NSURL fileURLWithPath:normalizedPath]];
}

- (void)reFetchFileNameCacheForPath:(NSString*)path
{
}

- (void)registerPath:(NSString*)path
{
	assert(_registeredDirectories);
	[_registeredDirectories addObject:[NSURL fileURLWithPath:path]];
	[FIFinderSyncController defaultController].directoryURLs = _registeredDirectories;
}

- (void)unregisterPath:(NSString*)path
{
	[_registeredDirectories removeObject:[NSURL fileURLWithPath:path]];
	[FIFinderSyncController defaultController].directoryURLs = _registeredDirectories;
}

- (void)setShareMenuTitle:(NSString*)title
{
	_shareMenuTitle = title;
}

- (void)connectionDidDie
{
	_shareMenuTitle = nil;
	
	[_registeredDirectories removeAllObjects];
	// For some reason the FIFinderSync cache doesn't seem to be cleared for the root item when
	// we reset the directoryURLs (seen on macOS 10.12 at least).
	// First setting it to the FS root and then setting it to nil seems to work around the issue.
	[FIFinderSyncController defaultController].directoryURLs = [NSSet setWithObject:[NSURL fileURLWithPath:@"/"]];
	// This will tell Finder that this extension isn't attached to any directory
	// until we can reconnect to the sync client.
	[FIFinderSyncController defaultController].directoryURLs = nil;
}

@end


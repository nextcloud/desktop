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
	//NSLog(@"FinderSync serverName %@", serverName);

	_syncClientProxy = [[SyncClientProxy alloc] initWithDelegate:self serverName:serverName];
	_registeredDirectories = [[NSMutableSet alloc] init];
	_strings = [[NSMutableDictionary alloc] init];

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

- (NSString*) selectedPathsSeparatedByRecordSeparator
{
	FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
	NSMutableString *string = [[NSMutableString alloc] init];
	[syncController.selectedItemURLs enumerateObjectsUsingBlock: ^(id obj, NSUInteger idx, BOOL *stop) {
		if (string.length > 0) {
			[string appendString:@"\x1e"]; // record separator
		}
		NSString* normalizedPath = [[obj path] decomposedStringWithCanonicalMapping];
		[string appendString:normalizedPath];
	}];
	return string;
}

- (NSMenu *)menuForMenuKind:(FIMenuKind)whichMenu
{
	FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
	NSMutableSet *rootPaths = [[NSMutableSet alloc] init];
	[syncController.directoryURLs enumerateObjectsUsingBlock: ^(id obj, BOOL *stop) {
		[rootPaths addObject:[obj path]];
	}];

	NSString *paths = [self selectedPathsSeparatedByRecordSeparator];
	// calling this IPC calls us back from client with several MENU_ITEM entries and then our askOnSocket returns again
	[_syncClientProxy askOnSocket:paths query:@"GET_MENU_ITEMS"];

	id contextMenuTitle = [_strings objectForKey:@"CONTEXT_MENU_TITLE"];
        if (contextMenuTitle && _menuItems.count != 0) {
            NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
            NSMenu *subMenu = [[NSMenu alloc] initWithTitle:@""];
            NSMenuItem *subMenuItem = [menu addItemWithTitle:contextMenuTitle action:nil keyEquivalent:@""];
            subMenuItem.submenu = subMenu;
            subMenuItem.image = [[NSBundle mainBundle] imageForResource:@"app.icns"];

            // There is an annoying bug in macOS (at least 10.13.3), it does not use/copy over the representedObject of a menu item
            // So we have to use tag instead.
            int idx = 0;
            for (NSArray *item in _menuItems) {
                NSMenuItem *actionItem = [subMenu addItemWithTitle:[item valueForKey:@"text"]
                                                            action:@selector(subMenuActionClicked:)
                                                     keyEquivalent:@""];
                [actionItem setTag:idx];
                [actionItem setTarget:self];
                NSString *flags = [item valueForKey:@"flags"]; // e.g. "d"
                if ([flags rangeOfString:@"d"].location != NSNotFound) {
                    [actionItem setEnabled:false];
                }
                idx++;
            }
            return menu;
        }
        return nil;
}

- (void)subMenuActionClicked:(id)sender {
	long idx = [(NSMenuItem*)sender tag];
	NSString *command = [[_menuItems objectAtIndex:idx] valueForKey:@"command"];
	NSString *paths = [self selectedPathsSeparatedByRecordSeparator];
	[_syncClientProxy askOnSocket:paths query:command];
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

- (void)setString:(NSString*)key value:(NSString*)value
{
	[_strings setObject:value forKey:key];
}

- (void)resetMenuItems
{
	_menuItems = [[NSMutableArray alloc] init];
}
- (void)addMenuItem:(NSDictionary *)item {
	[_menuItems addObject:item];
}

- (void)connectionDidDie
{
	[_strings removeAllObjects];
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


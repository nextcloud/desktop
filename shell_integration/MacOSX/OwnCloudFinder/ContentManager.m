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

#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <objc/runtime.h>
#import "ContentManager.h"
#import "MenuManager.h"
#import "RequestManager.h"
#import "IconCache.h"

static OwnCloudFinderContentManager* sharedInstance = nil;

@implementation OwnCloudFinderContentManager
- init
{
	self = [super init];

	if (self)
	{
		_fileNamesCache = [[NSMutableDictionary alloc] init];
		_oldFileNamesCache = [[NSMutableDictionary alloc] init];
		_fileIconsEnabled = TRUE;
		_hasChangedContent = TRUE;
		[self loadIconResources];
	}

	return self;
}

- (void)dealloc
{
	[self removeAllIcons];
	[_fileNamesCache release];
	[_oldFileNamesCache release];
	sharedInstance = nil;

	[super dealloc];
}

+ (OwnCloudFinderContentManager*)sharedInstance
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

- (void)loadIconResources
{
	NSBundle *extBundle = [NSBundle bundleForClass:[self class]];

	_icnOk   = [[IconCache sharedInstance] registerIcon:[extBundle imageForResource:@"ok.icns"]];
	_icnSync = [[IconCache sharedInstance] registerIcon:[extBundle imageForResource:@"sync.icns"]];
	_icnWarn = [[IconCache sharedInstance] registerIcon:[extBundle imageForResource:@"warning.icns"]];
	_icnErr  = [[IconCache sharedInstance] registerIcon:[extBundle imageForResource:@"error.icns"]];
	_icnOkSwm   = [[IconCache sharedInstance] registerIcon:[extBundle imageForResource:@"ok_swm.icns"]];
	_icnSyncSwm = [[IconCache sharedInstance] registerIcon:[extBundle imageForResource:@"sync_swm.icns"]];
	_icnWarnSwm = [[IconCache sharedInstance] registerIcon:[extBundle imageForResource:@"warning_swm.icns"]];
	_icnErrSwm  = [[IconCache sharedInstance] registerIcon:[extBundle imageForResource:@"error_swm.icns"]];

	// NSLog(@"Icon ok: %@ identifier: %d from bundle %@", [extBundle imageForResource:@"ok.icns"], [_icnOk intValue], extBundle);
}

- (void)enableFileIcons:(BOOL)enable
{
	_fileIconsEnabled = enable;

	[self repaintAllWindows];
}

- (void)setResultForPath:(NSString*)path result:(NSString*)result
{
	if (_icnOk == nil) {
		// no icon resource path registered yet
		return;
	}

	NSNumber *res;
	res = [NSNumber numberWithInt:0];

	if( [result isEqualToString:@"OK"] ) {
		res = _icnOk;
	} else if( [result isEqualToString:@"SYNC"] || [result isEqualToString:@"NEW"] ) {
		res = _icnSync;
	} else if( [result isEqualToString:@"IGNORE"]) {
		res = _icnWarn;
	} else if( [result isEqualToString:@"ERROR"]) {
		res = _icnErr;
	} else if( [result isEqualToString:@"OK+SWM"] ) {
		res = _icnOkSwm;
	} else if( [result isEqualToString:@"SYNC+SWM"] || [result isEqualToString:@"NEW+SWM"] ) {
		res = _icnSyncSwm;
	} else if( [result isEqualToString:@"IGNORE+SWM"]) {
		res = _icnWarnSwm;
	} else if( [result isEqualToString:@"ERROR+SWM"]) {
		res = _icnErrSwm;
	}else if( [result isEqualToString:@"NOP"]) {
		// Nothing.
	} else {
		NSLog(@"SyncState: Unknown status code %@", result);
	}
	
	NSString* normalizedPath = [path decomposedStringWithCanonicalMapping];

    if (![_fileNamesCache objectForKey:normalizedPath] || ![[_fileNamesCache objectForKey:normalizedPath] isEqualTo:res]) {
		[_fileNamesCache setObject:res forKey:normalizedPath];
		//NSLog(@"SET value %d %@", [res intValue], normalizedPath);
		_hasChangedContent = YES;
		[self performSelector:@selector(repaintAllWindowsIfNeeded) withObject:0 afterDelay:1.0]; // 1 sec
	}
}

- (NSNumber*)iconByPath:(NSString*)path isDirectory:(BOOL)isDir
{
	//NSLog(@"%@ %@", NSStringFromSelector(_cmd), path);
	if (!_fileIconsEnabled)
	{
		NSLog(@"SyncState: Icons are NOT ENABLED!");
		// return nil;
	}

	if( path == nil ) {
		NSNumber *res = [NSNumber numberWithInt:0];
		return res;
	}
	NSString* normalizedPath = [path decomposedStringWithCanonicalMapping];

	if (![[OwnCloudFinderRequestManager sharedInstance] isRegisteredPath:normalizedPath isDirectory:isDir]) {
		return [NSNumber numberWithInt:0];
	}
	
	NSNumber* result = [_fileNamesCache objectForKey:normalizedPath];
	// NSLog(@"XXXXXXX Asking for icon for path %@ = %d",normalizedPath, [result intValue]);
	
	if( result == nil ) {
		result = [NSNumber numberWithInt:0];
		// Set 0 into the cache, meaning "don't have an icon, but already requested it"
		[_fileNamesCache setObject:result forKey:normalizedPath];
		// start the async call
		[[OwnCloudFinderRequestManager sharedInstance] askForIcon:normalizedPath isDirectory:isDir];
	}
	if ([result intValue] == 0) {
		// Show the old state while we wait for the new one
		NSNumber* oldResult = [_oldFileNamesCache objectForKey:normalizedPath];
		if (oldResult)
			result = oldResult;
	}
	// NSLog(@"iconByPath return value %d", [result intValue]);

	return result;
}

// Clears the entries from the hash to make it call again home to the desktop client.
- (void)clearFileNameCache
{
	[_fileNamesCache release];
	_fileNamesCache = [[NSMutableDictionary alloc] init];
	[_oldFileNamesCache removeAllObjects];
}

- (void)reFetchFileNameCacheForPath:(NSString*)path
{
	//NSLog(@"%@", NSStringFromSelector(_cmd));

	// We won't request the new state if if finds the path in _fileNamesCache
	// Move all entries to _oldFileNamesCache so that the get re-requested, but
	// still available while we refill the cache
	[_oldFileNamesCache addEntriesFromDictionary:_fileNamesCache];
	[_fileNamesCache removeAllObjects];

	[self repaintAllWindows];
}


- (void)removeAllIcons
{
	[_fileNamesCache removeAllObjects];
	[_oldFileNamesCache removeAllObjects];

	[self repaintAllWindows];
}

- (void)repaintAllWindowsIfNeeded
{
	if (!_hasChangedContent) {
		//NSLog(@"%@ Repaint scheduled but not needed", NSStringFromSelector(_cmd));
		return;
	}

	_hasChangedContent = NO;
	[self repaintAllWindows];
}

- (void)repaintAllWindows
{
	//NSLog(@"%@", NSStringFromSelector(_cmd));
	NSArray* windows = [[NSApplication sharedApplication] windows];

	for (int i = 0; i < [windows count]; i++)
	{
		NSWindow* window = [windows objectAtIndex:i];

		if (![window isVisible])
		{
			continue;
		}

		MenuManager* menuManager = [MenuManager sharedInstance];
		OwnCloudFinderRequestManager* requestManager = [OwnCloudFinderRequestManager sharedInstance];

		if ([[window className] isEqualToString:@"TBrowserWindow"])
		{
			NSObject* browserWindowController = [window browserWindowController];

			BOOL repaintWindow = YES;

			NSString* filterFolder = [requestManager filterFolder];

			if (filterFolder)
			{
				repaintWindow = NO;

				struct TFENodeVector* targetPath;

				if ([browserWindowController respondsToSelector:@selector(targetPath)])
				{
					// 10.7 & 10.8
					targetPath = [browserWindowController targetPath];
				}
				else if ([browserWindowController respondsToSelector:@selector(activeContainer)])
				{
					// 10.9
					targetPath = [[browserWindowController activeContainer] targetPath];
				}
				else
				{
					NSLog(@"SyncState: refreshing icon badges failed");

					return;
				}

				NSArray* folderPaths = [menuManager pathsForNodes:targetPath];

				for (NSString* folderPath in folderPaths)
				{
					if ([folderPath hasPrefix:filterFolder] || [filterFolder hasPrefix:folderPath])
					{
						repaintWindow = YES;

						break;
					}
				}
			}

			if (repaintWindow)
			{
				if ([browserWindowController respondsToSelector:@selector(browserViewController)])
				{
					// 10.7 & 10.8
					NSObject* browserViewController = [browserWindowController browserViewController];

					NSObject* browserView = [browserViewController browserView];

					dispatch_async(dispatch_get_main_queue(), ^{[browserView setNeedsDisplay:YES];});
				}
				else if ([browserWindowController respondsToSelector:@selector(activeBrowserViewController)])
				{
					// 10.9
					NSObject* browserViewController = [browserWindowController activeBrowserViewController];

					NSObject* browserView = [browserViewController browserView];

					if ([browserView isKindOfClass:(id)objc_getClass("TListView")])
					{
						// List or Coverflow View
						[self setNeedsDisplayForListView:browserView];
					}
					else
					{
						// Icon or Column View
						dispatch_async(dispatch_get_main_queue(), ^{[browserView setNeedsDisplay:YES];});
					}
				}
				else
				{
					NSLog(@"SyncState: refreshing icon badges failed");

					return;
				}
			}
		}
	}
}

- (void)setIcons:(NSDictionary*)iconDictionary filterByFolder:(NSString*)filterFolder
{
	NSLog(@"%@", NSStringFromSelector(_cmd));
	for (NSString* path in iconDictionary)
	{
		if (filterFolder && ![path hasPrefix:filterFolder])
		{
			continue;
		}

		NSString* normalizedPath = [path decomposedStringWithCanonicalMapping];
		NSNumber* iconId = [iconDictionary objectForKey:path];

		if ([iconId intValue] == -1)
		{
			[_fileNamesCache removeObjectForKey:normalizedPath];
		}
		else
		{
			[_oldFileNamesCache removeObjectForKey:normalizedPath];
			[_fileNamesCache setObject:iconId forKey:normalizedPath];
		}
	}

	[self repaintAllWindows];
}

- (void)setNeedsDisplayForListView:(NSView*)view
{
	NSArray* subviews = [view subviews];

	for (int i = 0; i < [subviews count]; i++)
	{
		NSView* subview = [subviews objectAtIndex:i];

		if ([subview isKindOfClass:(id)objc_getClass("TListRowView")])
		{
			[self setNeedsDisplayForListView:subview];
		}
		else if ([subview isKindOfClass:(id)objc_getClass("TListNameCellView")])
		{
			dispatch_async(dispatch_get_main_queue(), ^{[subview setNeedsDisplay:YES];});
		}
	}
}

@end

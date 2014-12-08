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

static ContentManager* sharedInstance = nil;

@implementation ContentManager
- init
{
	self = [super init];

	if (self)
	{
		_fileNamesCache = [[NSMutableDictionary alloc] init];
		_fileIconsEnabled = TRUE;
		_hasChangedContent = TRUE;
	}

	return self;
}

- (void)dealloc
{
	[self removeAllIcons];
	[_fileNamesCache release];
	sharedInstance = nil;

	[super dealloc];
}

+ (ContentManager*)sharedInstance
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

- (void)loadIconResourcePath:(NSString*)path
{
	NSString *base = path;

	_icnOk   = [[IconCache sharedInstance] registerIcon:[base stringByAppendingString:@"ok.icns"]];
	_icnSync = [[IconCache sharedInstance] registerIcon:[base stringByAppendingString:@"sync.icns"]];
	_icnWarn = [[IconCache sharedInstance] registerIcon:[base stringByAppendingString:@"warning.icns"]];
	_icnErr  = [[IconCache sharedInstance] registerIcon:[base stringByAppendingString:@"error.icns"]];
	_icnOkSwm   = [[IconCache sharedInstance] registerIcon:[base stringByAppendingString:@"ok_swm.icns"]];
	_icnSyncSwm = [[IconCache sharedInstance] registerIcon:[base stringByAppendingString:@"sync_swm.icns"]];
	_icnWarnSwm = [[IconCache sharedInstance] registerIcon:[base stringByAppendingString:@"warning_swm.icns"]];
	_icnErrSwm  = [[IconCache sharedInstance] registerIcon:[base stringByAppendingString:@"error_swm.icns"]];

	NSLog(@"Icon ok identifier: %d from %@", [_icnOk intValue], [base stringByAppendingString:@"ok.icns"]);
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
		NSLog(@"Unknown status code %@", result);
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
		NSLog(@"Icons are NOT ENABLED!");
		// return nil;
	}

	if( path == nil ) {
		NSNumber *res = [NSNumber numberWithInt:0];
		return res;
	}
	NSString* normalizedPath = [path decomposedStringWithCanonicalMapping];

	if (![[RequestManager sharedInstance] isRegisteredPath:normalizedPath isDirectory:isDir]) {
		return [NSNumber numberWithInt:0];
	}
	
	NSNumber* result = [_fileNamesCache objectForKey:normalizedPath];
	// NSLog(@"XXXXXXX Asking for icon for path %@ = %d",normalizedPath, [result intValue]);
	
	if( result == nil ) {
		// start the async call
		NSNumber *askState = [[RequestManager sharedInstance] askForIcon:normalizedPath isDirectory:isDir];
		[_fileNamesCache setObject:askState forKey:normalizedPath];

		result = [NSNumber numberWithInt:0];
	} else if( [result intValue] == -1 ) {
		// the socket call is underways.
		result = [NSNumber numberWithInt:0];
	} else {
		// there is a proper icon index
	}
    // NSLog(@"iconByPath return value %d", [result intValue]);

	return result;
}

// called as a result of an UPDATE_VIEW message.
// it clears the entries from the hash to make it call again home to the desktop client.
- (void)clearFileNameCacheForPath:(NSString*)path
{
	NSLog(@"%@", NSStringFromSelector(_cmd));
	NSMutableArray *keysToDelete = [NSMutableArray array];
	
	if( path != nil ) {
		for (id p in [_fileNamesCache keyEnumerator]) {
			//do stuff with obj
			if ( [p hasPrefix:path] ) {
				[keysToDelete addObject:p];
			}
		}
	} else {
		// clear the entire fileNameCache
		[_fileNamesCache release];
		_fileNamesCache = [[NSMutableDictionary alloc] init];
		return;
	}
	
	if( [keysToDelete count] > 0 ) {
		NSLog( @"Entries to delete: %lu", (unsigned long)[keysToDelete count]);
		[_fileNamesCache removeObjectsForKeys:keysToDelete];
	}
}

- (void)reFetchFileNameCacheForPath:(NSString*)path
{
	 NSLog(@"%@", NSStringFromSelector(_cmd));

	for (id p in [_fileNamesCache keyEnumerator]) {
		if ( path && [p hasPrefix:path] ) {
			[[RequestManager sharedInstance] askForIcon:p isDirectory:false]; // FIXME isDirectory parameter
			//[_fileNamesCache setObject:askState forKey:p]; We don't do this since we want to keep the old icon meanwhile
			//NSLog(@"%@ %@", NSStringFromSelector(_cmd), p);
		}
	}

	// Ask for directory itself
	if ([path hasSuffix:@"/"]) {
		path = [path substringToIndex:path.length - 1];
	}
	[[RequestManager sharedInstance] askForIcon:path isDirectory:true];
	//NSLog(@"%@ %@", NSStringFromSelector(_cmd), path);
}


- (void)removeAllIcons
{
	[_fileNamesCache removeAllObjects];

	[self repaintAllWindows];
}

- (void)removeIcons:(NSArray*)paths
{
	for (NSString* path in paths)
	{
		NSString* normalizedPath = [path decomposedStringWithCanonicalMapping];

		[_fileNamesCache removeObjectForKey:normalizedPath];
	}

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
	NSLog(@"%@", NSStringFromSelector(_cmd));
	NSArray* windows = [[NSApplication sharedApplication] windows];

	for (int i = 0; i < [windows count]; i++)
	{
		NSWindow* window = [windows objectAtIndex:i];

		if (![window isVisible])
		{
			continue;
		}

		MenuManager* menuManager = [MenuManager sharedInstance];
		RequestManager* requestManager = [RequestManager sharedInstance];

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
					NSLog(@"OwnCloudFinder: refreshing icon badges failed");

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
					NSLog(@"OwnCloudFinder: refreshing icon badges failed");

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

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
		
		[[IconCache sharedInstance] registerIcon:@"/Users/mackie/owncloud.com/mirall/shell_integration/icons/icns/ok.icns"];
		[[IconCache sharedInstance] registerIcon:@"/Users/mackie/owncloud.com/mirall/shell_integration/icons/icns/sync.icns"];
		[[IconCache sharedInstance] registerIcon:@"/Users/mackie/owncloud.com/mirall/shell_integration/icons/icns/sync.icns"];
		[[IconCache sharedInstance] registerIcon:@"/Users/mackie/owncloud.com/mirall/shell_integration/icons/icns/sync.icns"];
		[[IconCache sharedInstance] registerIcon:@"/Users/mackie/owncloud.com/mirall/shell_integration/icons/icns/sync.icns"];
		
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

- (void)enableFileIcons:(BOOL)enable
{
	_fileIconsEnabled = enable;

	[self repaintAllWindows];
}

- (void)setResultForPath:(NSString*)path result:(NSString*)result
{
	int res = 0; // NOP
	if( [result isEqualToString:@"OK"] ) {
		res = 1;
	} else if( [result isEqualToString:@"NEED_SYNC"]) {
		res = 2;
	} else if( [result isEqualToString:@"IGNORE"]) {
		res = 3;
	} else if( [result isEqualToString:@"ERROR"]) {
		res = 4;
	}else if( [result isEqualToString:@"SHARED"]) {
		res = 5;
	}else if( [result isEqualToString:@"NOP"]) {
		// Nothing.
	} else {
		NSLog(@"Unknown status code %@", result);
	}
	NSString* normalizedPath = [path decomposedStringWithCanonicalMapping];
	[_fileNamesCache setObject:[NSNumber numberWithInt:res] forKey:normalizedPath];
	NSLog(@"SET value %d", res);
	
	[self repaintAllWindows];
}

- (NSNumber*)iconByPath:(NSString*)path isDirectory:(NSNumber*)isDir
{
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
	
	NSNumber* result = [_fileNamesCache objectForKey:normalizedPath];
	NSLog(@"XXXXXXX Asking for icon for path %@ = %d",path, [result intValue]);
	
	if( result == nil ) {
		// start the async call
		NSNumber* minusOne = [[NSNumber alloc] initWithInt:-1];
		[_fileNamesCache setObject:minusOne forKey:normalizedPath];
		[[RequestManager sharedInstance] askForIcon:normalizedPath isDirectory:isDir];
		result = [NSNumber numberWithInt:0];
	} else if( [result intValue] == -1 ) {
		// the socket call is underways.
		result = [NSNumber numberWithInt:0];
	} else {
		// there is a proper icon index
	}
    NSLog(@"iconByPath return value %d", [result intValue]);
	
	return result;
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

- (void)repaintAllWindows
{
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
					NSLog(@"LiferayNativityFinder: refreshing icon badges failed");

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
					NSLog(@"LiferayNativityFinder: refreshing icon badges failed");

					return;
				}
			}
		}
	}
}

- (void)setIcons:(NSDictionary*)iconDictionary filterByFolder:(NSString*)filterFolder
{
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
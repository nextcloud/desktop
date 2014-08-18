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

#import "MenuManager.h"
#import "Finder/Finder.h"
#import "RequestManager.h"

@implementation MenuManager

static MenuManager* sharedInstance = nil;

+ (MenuManager*)sharedInstance
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

- init
{
	return [super init];
}

- (void)addChildrenSubMenuItems:(NSMenuItem*)parentMenuItem withChildren:(NSArray*)menuItemsDictionaries forFiles:(NSArray*)files
{
	NSMenu* menu = [[NSMenu alloc] init];

	for (int i = 0; i < [menuItemsDictionaries count]; ++i)
	{
		NSDictionary* menuItemDictionary = [menuItemsDictionaries objectAtIndex:i];

		NSString* submenuTitle = [menuItemDictionary objectForKey:@"title"];
		BOOL enabled = [[menuItemDictionary objectForKey:@"enabled"] boolValue];
		NSString* uuid = [menuItemDictionary objectForKey:@"uuid"];
		NSArray* childrenSubMenuItems = (NSArray*)[menuItemDictionary objectForKey:@"contextMenuItems"];

		if ([submenuTitle isEqualToString:@"_SEPARATOR_"])
		{
			[menu addItem:[NSMenuItem separatorItem]];
		}
		else if (childrenSubMenuItems != nil && [childrenSubMenuItems count] != 0)
		{
			NSMenuItem* submenuItem = [menu addItemWithTitle:submenuTitle action:nil keyEquivalent:@""];

			[self addChildrenSubMenuItems:submenuItem withChildren:childrenSubMenuItems forFiles:files];
		}
		else
		{
			[self createActionMenuItemIn:menu withTitle:submenuTitle withIndex:i enabled:enabled withUuid:uuid forFiles:files];
		}
	}

	[parentMenuItem setSubmenu:menu];

	[menu release];
}

- (void)addItemsToMenu:(TContextMenu*)menu forFiles:(NSArray*)files
{
	// no menu items for now:
	return;

	// NSArray* menuItemsArray = [[RequestManager sharedInstance] menuItemsForFiles:files];
	NSArray* menuItemsArray;

	if (menuItemsArray == nil)
	{
		return;
	}

	if ([menuItemsArray count] == 0)
	{
		return;
	}

	NSInteger menuIndex = 4;

	BOOL hasSeparatorBefore = [[menu itemAtIndex:menuIndex - 1] isSeparatorItem];

	if (!hasSeparatorBefore)
	{
		[menu insertItem:[NSMenuItem separatorItem] atIndex:menuIndex];
	}

	for (int i = 0; i < [menuItemsArray count]; ++i)
	{
		NSDictionary* menuItemDictionary = [menuItemsArray objectAtIndex:i];

		NSString* mainMenuTitle = [menuItemDictionary objectForKey:@"title"];

		if ([mainMenuTitle isEqualToString:@""])
		{
			continue;
		}

		menuIndex++;

		BOOL enabled = [[menuItemDictionary objectForKey:@"enabled"] boolValue];
		NSString* uuid = [menuItemDictionary objectForKey:@"uuid"];
		NSArray* childrenSubMenuItems = (NSArray*)[menuItemDictionary objectForKey:@"contextMenuItems"];

		if (childrenSubMenuItems != nil && [childrenSubMenuItems count] != 0)
		{
			NSMenuItem* mainMenuItem = [menu insertItemWithTitle:mainMenuTitle action:nil keyEquivalent:@"" atIndex:menuIndex];

			[self addChildrenSubMenuItems:mainMenuItem withChildren:childrenSubMenuItems forFiles:files];
		}
		else
		{
			[self createActionMenuItemIn:menu withTitle:mainMenuTitle withIndex:menuIndex enabled:enabled withUuid:uuid forFiles:files];
		}
	}

	BOOL hasSeparatorAfter = [[menu itemAtIndex:menuIndex + 1] isSeparatorItem];

	if (!hasSeparatorAfter)
	{
		[menu insertItem:[NSMenuItem separatorItem] atIndex:menuIndex + 1];
	}
}

- (void)createActionMenuItemIn:(NSMenu*)menu withTitle:(NSString*)title withIndex:(NSInteger*)index enabled:(BOOL)enabled withUuid:(NSString*)uuid forFiles:(NSArray*)files
{
	NSMenuItem* mainMenuItem = [menu insertItemWithTitle:title action:@selector(menuItemClicked:) keyEquivalent:@"" atIndex:index];

	if (enabled)
	{
		[mainMenuItem setTarget:self];
	}

	NSDictionary* menuActionDictionary = [[NSMutableDictionary alloc] init];
	[menuActionDictionary setValue:uuid forKey:@"uuid"];
	NSMutableArray* filesArray = [files copy];
	[menuActionDictionary setValue:filesArray forKey:@"files"];

	[mainMenuItem setRepresentedObject:menuActionDictionary];

	[filesArray release];
	[menuActionDictionary release];
}

- (void)menuItemClicked:(id)param
{
	[[RequestManager sharedInstance] menuItemClicked:[param representedObject]];
}

- (NSArray*)pathsForNodes:(const struct TFENodeVector*)nodes
{
	struct TFENode* start = nodes->_M_impl._M_start;
	struct TFENode* end = nodes->_M_impl._M_finish;

	int count = end - start;

	NSMutableArray* selectedItems = [[NSMutableArray alloc] initWithCapacity:count];
	struct TFENode* current;

	for (current = start; current < end; ++current)
	{
		FINode* node = (FINode*)[NSClassFromString(@"FINode") nodeFromNodeRef:current->fNodeRef];

		NSString* path = [[node previewItemURL] path];

		if (path)
		{
			[selectedItems addObject:path];
		}
	}

	return [selectedItems autorelease];
}

@end
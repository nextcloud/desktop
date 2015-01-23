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

#import "ContextMenuHandlers.h"
#import "MenuManager.h"

@class TIconViewController;

@implementation NSObject (ContextMenuHandlers)

+ (void)OCContextMenuHandlers_addViewSpecificStuffToMenu:(id)arg1 browserViewController:(id)arg2 context:(unsigned int)arg3 // 10.7 & 10.8
{
	[self OCContextMenuHandlers_addViewSpecificStuffToMenu:arg1 browserViewController:arg2 context:arg3];
	NSLog(@"OCContextMenuHandlers_addViewSpecificStuffToMenu 10.7/10.8 %@ %@ %d", arg1, arg2, arg3);
	MenuManager* menuManager = [MenuManager sharedInstance];
	if (menuManager.menuItems.count > 0)
	{
		[menuManager addItemsToMenu:arg1 forFiles:menuManager.menuItems];
		[menuManager.menuItems removeAllObjects];
	}
}

+ (void)OCContextMenuHandlers_addViewSpecificStuffToMenu:(id)arg1 clickedView:(id)arg2 browserViewController:(id)arg3 context:(unsigned int)arg4 // 10.9
{
	[self OCContextMenuHandlers_addViewSpecificStuffToMenu:arg1 clickedView:arg2 browserViewController:arg3 context:arg4];
	NSLog(@"OCContextMenuHandlers_addViewSpecificStuffToMenu 10.9 %@ %@ %@ %d", arg1, arg2, arg3, arg4);
	MenuManager* menuManager = [MenuManager sharedInstance];
	if (menuManager.menuItems.count > 0)
	{
		[menuManager addItemsToMenu:arg1 forFiles:menuManager.menuItems];
		[menuManager.menuItems removeAllObjects];
	}
}

+ (void)OCContextMenuHandlers_handleContextMenuCommon:(unsigned int)arg1 nodes:(const struct TFENodeVector*)arg2 event:(id)arg3 view:(id)arg4 windowController:(id)arg5 addPlugIns:(BOOL)arg6   // 10.7
{
	MenuManager* menuManager = [MenuManager sharedInstance];
	NSLog(@"ContextMenuHandlers_handleContextMenuCommon");
	menuManager.menuItems = (NSMutableArray*)[menuManager pathsForNodes:arg2];
	[self OCContextMenuHandlers_handleContextMenuCommon:arg1 nodes:arg2 event:arg3 view:arg4 windowController:arg5 addPlugIns:arg6];
}

+ (void)OCContextMenuHandlers_handleContextMenuCommon:(unsigned int)arg1 nodes:(const struct TFENodeVector*)arg2 event:(id)arg3 view:(id)arg4 browserController:(id)arg5 addPlugIns:(BOOL)arg6  // 10.8
{
	MenuManager* menuManager = [MenuManager sharedInstance];
	NSLog(@"ContextMenuHandlers_handleContextMenuCommon");
	menuManager.menuItems = (NSMutableArray*)[menuManager pathsForNodes:arg2];
	[self OCContextMenuHandlers_handleContextMenuCommon:arg1 nodes:arg2 event:arg3 view:arg4 browserController:arg5 addPlugIns:arg6];
}

+ (void)OCContextMenuHandlers_handleContextMenuCommon:(unsigned int)arg1 nodes:(const struct TFENodeVector*)arg2 event:(id)arg3 clickedView:(id)arg4 browserViewController:(id)arg5 addPlugIns:(BOOL)arg6  // 10.9
{
	MenuManager* menuManager = [MenuManager sharedInstance];
	NSLog(@"ContextMenuHandlers_handleContextMenuCommon");
	menuManager.menuItems = (NSMutableArray*)[menuManager pathsForNodes:arg2];
	[self OCContextMenuHandlers_handleContextMenuCommon:arg1 nodes:arg2 event:arg3 clickedView:arg4 browserViewController:arg5 addPlugIns:arg6];
}

- (void)OCContextMenuHandlers_configureWithNodes:(const struct TFENodeVector*)arg1 windowController:(id)arg2 container:(BOOL)arg3   // 10.7
{
	[self OCContextMenuHandlers_configureWithNodes:arg1 windowController:arg2 container:arg3];
	NSLog(@"ContextMenuHandlers_configureWithNodes");
	TContextMenu* realSelf = (TContextMenu*)self;
	MenuManager* menuManager = [MenuManager sharedInstance];
	NSArray* selectedItems = [menuManager pathsForNodes:arg1];
	[menuManager addItemsToMenu:realSelf forFiles:selectedItems];
}

- (void)OCContextMenuHandlers_configureWithNodes:(const struct TFENodeVector*)arg1 browserController:(id)arg2 container:(BOOL)arg3  // 10.8
{
	[self OCContextMenuHandlers_configureWithNodes:arg1 browserController:arg2 container:arg3];
	NSLog(@"ContextMenuHandlers_configureWithNodes");
	TContextMenu* realSelf = (TContextMenu*)self;
	MenuManager* menuManager = [MenuManager sharedInstance];

	NSArray* selectedItems = [menuManager pathsForNodes:arg1];
	[menuManager addItemsToMenu:realSelf forFiles:selectedItems];
}

- (void)OCContextMenuHandlers_configureFromMenuNeedsUpdate:(id)arg1 clickedView:(id)arg2 container:(BOOL)arg3 event:(id)arg4 selectedNodes:(const struct TFENodeVector *)arg5  // 10.9
{
	[self OCContextMenuHandlers_configureFromMenuNeedsUpdate:arg1 clickedView:arg2 container:arg3 event:arg4 selectedNodes:arg5];  // 10.8
	NSLog(@"ContextMenuHandlers_configureFromMenuNeedsUpdate");
	TContextMenu* realSelf = (TContextMenu*)self;
	MenuManager* menuManager = [MenuManager sharedInstance];
	NSArray* selectedItems = [menuManager pathsForNodes:arg5];
	[menuManager addItemsToMenu:realSelf forFiles:selectedItems];
}

@end
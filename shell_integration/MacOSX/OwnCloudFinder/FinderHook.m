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
#import "FinderHook.h"
#import "IconCache.h"
#import "objc/objc-class.h"
#import "RequestManager.h"

static BOOL installed = NO;

@implementation FinderHook

+ (void)hookClassMethod:(SEL)oldSelector inClass:(NSString*)className toCallToTheNewMethod:(SEL)newSelector
{
	Class hookedClass = NSClassFromString(className);
	Method oldMethod = class_getClassMethod(hookedClass, oldSelector);
	Method newMethod = class_getClassMethod(hookedClass, newSelector);

	method_exchangeImplementations(newMethod, oldMethod);
}

+ (void)hookMethod:(SEL)oldSelector inClass:(NSString*)className toCallToTheNewMethod:(SEL)newSelector
{
	Class hookedClass = NSClassFromString(className);
	Method oldMethod = class_getInstanceMethod(hookedClass, oldSelector);
	Method newMethod = class_getInstanceMethod(hookedClass, newSelector);

	method_exchangeImplementations(newMethod, oldMethod);
}

+ (void)install
{
	if (installed)
	{
		// NSLog(@"SyncStateFinder: already installed");

		return;
	}

	// NSLog(@"SyncStateFinder: installing SyncState Shell extension");

	[OwnCloudFinderRequestManager sharedInstance];

	// Icons
	[self hookMethod:@selector(drawImage:) inClass:@"IKImageBrowserCell" toCallToTheNewMethod:@selector(OCIconOverlayHandlers_IKImageBrowserCell_drawImage:)]; // 10.7 & 10.8 & 10.9 (Icon View arrange by name)

	[self hookMethod:@selector(drawImage:) inClass:@"IKFinderReflectiveIconCell" toCallToTheNewMethod:@selector(OCIconOverlayHandlers_IKFinderReflectiveIconCell_drawImage:)]; // 10.7 & 10.8 & 10.9 (Icon View arrange by everything else)

	[self hookMethod:@selector(drawIconWithFrame:) inClass:@"TColumnCell" toCallToTheNewMethod:@selector(OCIconOverlayHandlers_drawIconWithFrame:)]; // 10.7 & 10.8 & 10.9 Column View

	[self hookMethod:@selector(drawRect:) inClass:@"TDimmableIconImageView" toCallToTheNewMethod:@selector(OCIconOverlayHandlers_drawRect:)]; // 10.9 (List and Coverflow Views)

	// Context Menus
	[self hookClassMethod:@selector(addViewSpecificStuffToMenu:browserViewController:context:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(OCContextMenuHandlers_addViewSpecificStuffToMenu:browserViewController:context:)]; // 10.7 & 10.8

	[self hookClassMethod:@selector(addViewSpecificStuffToMenu:clickedView:browserViewController:context:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(OCContextMenuHandlers_addViewSpecificStuffToMenu:clickedView:browserViewController:context:)]; // 10.9

	[self hookClassMethod:@selector(handleContextMenuCommon:nodes:event:view:windowController:addPlugIns:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(OCContextMenuHandlers_handleContextMenuCommon:nodes:event:view:windowController:addPlugIns:)]; // 10.7

	[self hookClassMethod:@selector(handleContextMenuCommon:nodes:event:view:browserController:addPlugIns:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(OCContextMenuHandlers_handleContextMenuCommon:nodes:event:view:browserController:addPlugIns:)]; // 10.8

	[self hookClassMethod:@selector(handleContextMenuCommon:nodes:event:clickedView:browserViewController:addPlugIns:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(OCContextMenuHandlers_handleContextMenuCommon:nodes:event:clickedView:browserViewController:addPlugIns:)]; // 10.9

	[self hookMethod:@selector(configureWithNodes:windowController:container:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(OCContextMenuHandlers_configureWithNodes:windowController:container:)]; // 10.7

	[self hookMethod:@selector(configureWithNodes:browserController:container:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(OCContextMenuHandlers_configureWithNodes:browserController:container:)]; // 10.8

	[self hookMethod:@selector(configureFromMenuNeedsUpdate:clickedView:container:event:selectedNodes:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(OCContextMenuHandlers_configureFromMenuNeedsUpdate:clickedView:container:event:selectedNodes:)]; // 10.9

	installed = YES;

    // NSLog(@"SyncStateFinder: installed");
}

+ (void)uninstall
{
	if (!installed)
	{
		// NSLog(@"SyncStateFinder: not installed");

		return;
	}

	// NSLog(@"SyncStateFinder: uninstalling");

	[[OwnCloudFinderContentManager sharedInstance] dealloc];

	[[IconCache sharedInstance] dealloc];

	[[OwnCloudFinderRequestManager sharedInstance] dealloc];

	// Icons
	[self hookMethod:@selector(OCIconOverlayHandlers_drawImage:) inClass:@"TIconViewCell" toCallToTheNewMethod:@selector(drawImage:)]; // 10.7 & 10.8 & 10.9

	[self hookMethod:@selector(OCIconOverlayHandlers_drawIconWithFrame:) inClass:@"TListViewIconAndTextCell" toCallToTheNewMethod:@selector(drawIconWithFrame:)]; // 10.7 & 10.8 & 10.9

	// Context Menus
	[self hookClassMethod:@selector(OCContextMenuHandlers_addViewSpecificStuffToMenu:browserViewController:context:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(addViewSpecificStuffToMenu:browserViewController:context:)]; // 10.7 & 10.8

	[self hookClassMethod:@selector(OCContextMenuHandlers_handleContextMenuCommon:nodes:event:view:windowController:addPlugIns:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(handleContextMenuCommon:nodes:event:view:windowController:addPlugIns:)]; // 10.7

	[self hookMethod:@selector(OCContextMenuHandlers_configureWithNodes:windowController:container:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(configureWithNodes:windowController:container:)]; // 10.7

	[self hookClassMethod:@selector(OCContextMenuHandlers_handleContextMenuCommon:nodes:event:view:browserController:addPlugIns:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(handleContextMenuCommon:nodes:event:view:browserController:addPlugIns:)]; // 10.8

	[self hookMethod:@selector(OCContextMenuHandlers_configureWithNodes:browserController:container:) inClass:@"TContextMenu" toCallToTheNewMethod:@selector(configureWithNodes:browserController:container:)]; // 10.8

	installed = NO;

    // NSLog(@"SyncStateFinder: uninstalled");
}

@end
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


#import <Cocoa/Cocoa.h>
#import <FinderSync/FinderSync.h>
#import "SyncClientProxy.h"

@interface FinderSync : FIFinderSync <SyncClientProxyDelegate>
{
	SyncClientProxy *_syncClientProxy;
	NSMutableSet *_registeredDirectories;
	NSString *_shareMenuTitle;
	NSString *_mirror_path;
	NSString *_fs_path;
	NSMutableDictionary *_strings;
	NSMutableArray *_menuItems;
	Boolean shouldBeChecked;
}

@end

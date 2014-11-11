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

#import "IconCache.h"

@implementation IconCache

static IconCache* sharedInstance = nil;

- init
{
	self = [super init];

	if (self)
	{
		_iconIdDictionary = [[NSMutableDictionary alloc] init];
		_iconPathDictionary = [[NSMutableDictionary alloc] init];
		_currentIconId = 0;
	}

	return self;
}

- (void)dealloc
{
	[_iconIdDictionary release];
	[_iconPathDictionary release];
	sharedInstance = nil;

	[super dealloc];
}

+ (IconCache*)sharedInstance
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

- (NSImage*)getIcon:(NSNumber*)iconId
{
	NSImage* image = [_iconIdDictionary objectForKey:iconId];

	return image;
}

- (NSNumber*)registerIcon:(NSString*)path
{
	if (path == nil)
	{
		return [NSNumber numberWithInt:-1];
	}

	NSImage* image = [[NSImage alloc]initWithContentsOfFile:path];

	if (image == nil)
	{
		NSLog(@"%@ Could not load %@", NSStringFromSelector(_cmd), path);
		return [NSNumber numberWithInt:-1];
	}

	NSNumber* iconId = [_iconPathDictionary objectForKey:path];

	if (iconId == nil)
	{
		_currentIconId++;

		iconId = [NSNumber numberWithInt:_currentIconId];

		[_iconPathDictionary setObject:iconId forKey:path];
	}

	[_iconIdDictionary setObject:image forKey:iconId];
	[image release];

	return iconId;
}

- (void)unregisterIcon:(NSNumber*)iconId
{
	NSString* path = @"";

	for (NSString* key in _iconPathDictionary)
	{
		NSNumber* value = [_iconPathDictionary objectForKey:key];

		if ([value isEqualToNumber:iconId])
		{
			path = key;

			break;
		}
	}

	[_iconPathDictionary removeObjectForKey:path];

	[_iconIdDictionary removeObjectForKey:iconId];
}

@end

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

#import <objc/runtime.h>
#import "ContentManager.h"
#import "IconCache.h"
#import "FinishedIconCache.h"
#import "IconOverlayHandlers.h"
#import "Finder/Finder.h"

@implementation NSObject (IconOverlayHandlers)

- (void)IconOverlayHandlers_drawIconWithFrame:(struct CGRect)arg1
{
	[self OCIconOverlayHandlers_drawIconWithFrame:arg1];

	NSURL* url = [[NSClassFromString(@"FINode") nodeFromNodeRef:[(TIconAndTextCell*)self node]->fNodeRef] previewItemURL];

    BOOL isDir;
	if ([[NSFileManager defaultManager] fileExistsAtPath: [url path] isDirectory:&isDir] == NO) {
		NSLog(@"ERROR: Could not determine file type of %@", [url path]);
		isDir = NO;
	}

	NSNumber* imageIndex = [[ContentManager sharedInstance] iconByPath:[url path] isDirectory:isDir];

	//NSLog(@"1 The icon index is %d", [imageIndex intValue]);
	if ([imageIndex intValue] > 0)
	{
		NSImage* image = [[IconCache sharedInstance] getIcon:imageIndex];

		if (image != nil)
		{
			struct CGRect arg2 = [(TIconViewCell*)self imageRectForBounds:arg1];

			[image drawInRect:NSMakeRect(arg2.origin.x, arg2.origin.y, arg2.size.width, arg2.size.height) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0 respectFlipped:TRUE hints:nil];
		}
	}
}

- (void)OCIconOverlayHandlers_IKImageBrowserCell_drawImage:(id)arg1
{
	IKImageWrapper*imageWrapper = [self OCIconOverlayHandlers_imageWrapper:arg1];

	[self OCIconOverlayHandlers_IKImageBrowserCell_drawImage:imageWrapper];
}

- (void)OCIconOverlayHandlers_IKFinderReflectiveIconCell_drawImage:(id)arg1
{
	IKImageWrapper*imageWrapper = [self OCIconOverlayHandlers_imageWrapper:arg1];

	[self OCIconOverlayHandlers_IKFinderReflectiveIconCell_drawImage:imageWrapper];
}

- (IKImageWrapper*)OCIconOverlayHandlers_imageWrapper:(id)arg1
{
	TIconViewCell* realSelf = (TIconViewCell*)self;
	FINode* node = (FINode*)[realSelf representedItem];

	NSURL* url = [node previewItemURL];

	BOOL isDir;
	if ([[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&isDir] == NO) {
		NSLog(@"ERROR: Could not determine file type of %@", [url path]);
		isDir = NO;
	}

	NSNumber* imageIndex = [[ContentManager sharedInstance] iconByPath:[url path] isDirectory:isDir];
	//NSLog(@"2 The icon index is %d %@ %@", [imageIndex intValue], [url path], isDir ? @"isDir" : @"");

	if ([imageIndex intValue] > 0)
	{
		NSImage* icon = [arg1 _nsImage];

		// Use the short term icon cache that possibly has the finished icon
		FinishedIconCache *finishedIconCache = [FinishedIconCache sharedInstance];
		NSImage *finishedImage = [finishedIconCache getIcon:[url path] overlayIconIndex:imageIndex width:[icon size].width height:[icon size].height];
		if (finishedImage) {
			//NSLog(@"X Got finished image from cache %@ %@", finishedImage, [url path]);
			return [[[IKImageWrapper alloc] initWithNSImage:finishedImage] autorelease];;
		} else {
			//NSLog(@"X Need to redraw %@", [url path]);
		}

		NSImage* iconimage = [[IconCache sharedInstance] getIcon:[NSNumber numberWithInt:[imageIndex intValue]]];

		if (iconimage != nil)
		{
			[icon lockFocus];

			CGContextRef myContext = [[NSGraphicsContext currentContext] graphicsPort];

			CGRect destRect = CGRectMake(0, 0, [icon size].width, [icon size].height);

			CGImageRef cgImage = [iconimage CGImageForProposedRect:&destRect
														   context:[NSGraphicsContext currentContext]
															 hints:nil];
			if (cgImage) {
				CGContextDrawImage(myContext, destRect, cgImage);
				//CGImageRelease(cgImage); // leak here? if we leave this code in, Finder crashes
				// But actually i'm not seeing a leak in Activity Monitor.. maybe it is not really leaking?
			} else {
				NSLog(@"No image given!!!!!11 %@", [url path]);
			}

			[icon unlockFocus];
		}

		// Insert into cache
		[finishedIconCache registerIcon:icon withFileName:[url path] overlayIconIndex:imageIndex width:[icon size].width height:[icon size].height];

		return [[[IKImageWrapper alloc] initWithNSImage:icon] autorelease];
	}
	else
	{
		return arg1;
	}
}

- (void)OCIconOverlayHandlers_drawRect:(struct CGRect)arg1
{
	[self OCIconOverlayHandlers_drawRect:arg1];

	NSView* supersuperview = [[(NSView*)self superview] superview];

	if ([supersuperview isKindOfClass:(id)objc_getClass("TListRowView")])
	{
		TListRowView *listRowView = (TListRowView*) supersuperview;
		FINode *fiNode;

		object_getInstanceVariable(listRowView, "_node", (void**)&fiNode);

		NSURL *url;

		if ([fiNode respondsToSelector:@selector(previewItemURL)])
		{
			url = [fiNode previewItemURL];
		}
		else {
			return;
		}
		
		BOOL isDir;
		if ([[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory: &isDir] == NO) {
			NSLog(@"ERROR: Could not determine file type of %@", [url path]);
			isDir = NO;
		}
		
		NSNumber* imageIndex = [[ContentManager sharedInstance] iconByPath:[url path] isDirectory:isDir];
		//NSLog(@"3 The icon index is %d", [imageIndex intValue]);

		if ([imageIndex intValue] > 0)
		{
			NSImage* image = [[IconCache sharedInstance] getIcon:imageIndex];

			if (image != nil)
			{
				[image drawInRect:NSMakeRect(arg1.origin.x, arg1.origin.y, arg1.size.width, arg1.size.height) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0 respectFlipped:TRUE hints:nil];
			}
		}

	}
}

@end
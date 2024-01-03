/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#import "fileproviderstorageuseenumerationobserver.h"

@implementation FileProviderStorageUseEnumerationObserver

- (instancetype)init
{
    self = [super init];
    if (self) {
        _materialisedItems = [NSSet set];
    }
    return self;
}

// NSFileProviderEnumerationObserver protocol methods
- (void)didEnumerateItems:(NSArray<id<NSFileProviderItem>> *)updatedItems
{
    NSMutableSet<id<NSFileProviderItem>> * const existingItems = self.materialisedItems.mutableCopy;

    for (const id<NSFileProviderItem> item in updatedItems) {
        NSLog(@"StorageUseEnumerationObserver: Enumerating %@ with size %llu",
              item.filename, item.documentSize.unsignedLongLongValue);
        [existingItems addObject:item];
    }

    _materialisedItems = existingItems.copy;
}

- (void)finishEnumeratingWithError:(NSError *)error
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self.enumerationFinishedHandler(error);
    });
}

- (void)finishEnumeratingUpToPage:(NSFileProviderPage)nextPage
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self.enumerationFinishedHandler(nil);
    });
}

@end

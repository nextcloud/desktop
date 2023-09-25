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

@interface FileProviderStorageUseEnumerationObserver ()

@property (readwrite) NSUInteger usage;

@end

@implementation FileProviderStorageUseEnumerationObserver

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.usage = 0ULL;
    }
    return self;
}

// NSFileProviderEnumerationObserver protocol methods
- (void)didEnumerateItems:(NSArray<id<NSFileProviderItem>> *)updatedItems
{
    for (const id<NSFileProviderItem> item in updatedItems) {
        NSLog(@"StorageUseEnumerationObserver: Enumerating %@ with size %llu", item.filename, item.documentSize.unsignedLongLongValue);
        self.usage += item.documentSize.unsignedLongLongValue;
    }
}

- (void)finishEnumeratingWithError:(NSError *)error
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self.enumerationFinishedHandler(nil, error);
    });
}

- (void)finishEnumeratingUpToPage:(NSFileProviderPage)nextPage
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self.enumerationFinishedHandler(@(self.usage), nil);
    });
}

@end

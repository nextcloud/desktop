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

@property (readwrite) NSNumber *usage;

@end

@implementation FileProviderStorageUseEnumerationObserver

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.usage = @(0);
    }
    return self;
}

// NSFileProviderEnumerationObserver protocol methods
- (void)didEnumerateItems:(NSArray<id<NSFileProviderItem>> *)updatedItems
{
    for (const id<NSFileProviderItem> item in updatedItems) {
        self.usage = @(self.usage.unsignedLongLongValue + item.documentSize.unsignedLongLongValue);
    }
}

- (void)finishEnumeratingWithError:(NSError *)error
{
    self.enumerationFinishedHandler(nil, error);
}

- (void)finishEnumeratingUpToPage:(NSFileProviderPage)nextPage
{
    self.enumerationFinishedHandler(self.usage, nil);
}

@end

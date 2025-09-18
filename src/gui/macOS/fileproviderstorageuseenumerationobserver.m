/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

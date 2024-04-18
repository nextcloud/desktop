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

#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>

typedef void(^UsageEnumerationFinishedHandler)(NSError *const error);

@interface FileProviderStorageUseEnumerationObserver : NSObject<NSFileProviderEnumerationObserver>

@property (readwrite, strong) UsageEnumerationFinishedHandler enumerationFinishedHandler;
@property (readonly) NSSet<id<NSFileProviderItem>> *materialisedItems;

@end

// SPDX-FileCopyrightText: 2023 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>

typedef void(^UsageEnumerationFinishedHandler)(NSError *const error);

@interface FileProviderStorageUseEnumerationObserver : NSObject<NSFileProviderEnumerationObserver>

@property (readwrite, strong) UsageEnumerationFinishedHandler enumerationFinishedHandler;
@property (readonly) NSSet<id<NSFileProviderItem>> *materialisedItems;

@end

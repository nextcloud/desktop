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


#import <FileProvider/FileProvider.h>

NS_ASSUME_NONNULL_BEGIN

@class FileProviderExtension;

@interface ClientCommunicationService : NSObject<NSFileProviderServiceSource>

@property (readonly) NSXPCListener *listener;
@property (readonly) FileProviderExtension *extension;

- (instancetype)initWithFileProviderExtension:(FileProviderExtension *)extension;

@end

NS_ASSUME_NONNULL_END

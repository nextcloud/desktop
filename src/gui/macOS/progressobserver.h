/*
* Copyright 2024 (c) Claudio Cambra <claudio.cambra@nextcloud.com>
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

NS_ASSUME_NONNULL_BEGIN

typedef void(^ProgressKVOChangeHandler)(NSProgress *const progress);

@interface ProgressObserver : NSObject

@property (readonly) NSProgress *progress;
@property (readwrite, copy) ProgressKVOChangeHandler progressKVOChangeHandler;

- (instancetype)initWithProgress:(NSProgress *)progress;

@end

NS_ASSUME_NONNULL_END

// SPDX-FileCopyrightText: 2024 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef void(^ProgressKVOChangeHandler)(NSProgress *const progress);

@interface ProgressObserver : NSObject

@property (readonly) NSProgress *progress;
@property (readwrite, copy) ProgressKVOChangeHandler progressKVOChangeHandler;

- (instancetype)initWithProgress:(NSProgress *)progress;

@end

NS_ASSUME_NONNULL_END

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

#import "progressobserver.h"

@implementation ProgressObserver

- (instancetype)initWithProgress:(NSProgress *)progress
{
    self = [super init];
    if (self) {
        _progress = progress;
        [_progress addObserver:self forKeyPath:@"totalUnitCount" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"completedUnitCount" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"cancelled" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"paused" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"fileTotalCount" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"fileCompletedCount" options:NSKeyValueObservingOptionNew context:nil];
    }
    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    self.progressKVOChangeHandler(self.progress);
}

@end

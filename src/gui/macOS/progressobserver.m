/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "progressobserver.h"

@implementation ProgressObserver

- (instancetype)initWithProgress:(NSProgress *)progress
{
    self = [super init];
    if (self) {
        _progress = progress;
        [_progress retain];
        [_progress addObserver:self forKeyPath:@"totalUnitCount" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"completedUnitCount" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"cancelled" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"paused" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"fileTotalCount" options:NSKeyValueObservingOptionNew context:nil];
        [_progress addObserver:self forKeyPath:@"fileCompletedCount" options:NSKeyValueObservingOptionNew context:nil];
    }
    return self;
}

- (void)dealloc
{
    [_progress removeObserver:self forKeyPath:@"totalUnitCount"];
    [_progress removeObserver:self forKeyPath:@"completedUnitCount"];
    [_progress removeObserver:self forKeyPath:@"cancelled"];
    [_progress removeObserver:self forKeyPath:@"paused"];
    [_progress removeObserver:self forKeyPath:@"fileTotalCount"];
    [_progress removeObserver:self forKeyPath:@"fileCompletedCount"];
    [_progress release];
    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    self.progressKVOChangeHandler(self.progress);
}

@end

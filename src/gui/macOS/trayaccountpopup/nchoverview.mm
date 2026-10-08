/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "nchoverview.h"

static_assert(__has_feature(objc_arc), "The native tray hover view requires ARC.");

@implementation NCHoverView

- (instancetype)init
{
    self = [super init];
    if (!self) return nil;
    self.wantsLayer = YES;
    self.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);
    self.translatesAutoresizingMaskIntoConstraints = NO;
    return self;
}

- (void)mouseEntered:(NSEvent *)event
{
    self.layer.backgroundColor = [NSColor.labelColor colorWithAlphaComponent:0.08].CGColor;
}

- (void)mouseExited:(NSEvent *)event
{
    self.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    auto trackingAreas = [self.trackingAreas copy];
    for (NSTrackingArea *ta in trackingAreas) {
        [self removeTrackingArea:ta];
    }

    auto trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
               owner:self
            userInfo:nil];
    [self addTrackingArea:trackingArea];
}

@end

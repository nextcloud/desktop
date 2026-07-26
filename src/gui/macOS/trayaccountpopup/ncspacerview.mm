/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncspacerview.h"

#import "trayaccountpopupmetrics.h"

@implementation NCSpacerView

- (instancetype)initWithHeight:(CGFloat)height
{
    return [self initWithHeight:height width:kPopupWidth];
}

- (instancetype)initWithHeight:(CGFloat)height width:(CGFloat)width
{
    self = [super init];
    if (!self) return nil;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [self.heightAnchor constraintEqualToConstant:height],
        [self.widthAnchor constraintEqualToConstant:width],
    ]];
    return self;
}

@end

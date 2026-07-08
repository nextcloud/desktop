/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncsectionheaderrow.h"

#import "trayaccountpopupmetrics.h"

@implementation NCSectionHeaderRow

- (instancetype)initWithTitle:(NSString *)title width:(CGFloat)width
{
    self = [super init];
    if (!self) return nil;
    self.translatesAutoresizingMaskIntoConstraints = NO;

    auto label = [NSTextField labelWithString:title];
    label.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    label.textColor = NSColor.secondaryLabelColor;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:label];

    [NSLayoutConstraint activateConstraints:@[
        [self.heightAnchor constraintEqualToConstant:kSectionHeaderHeight],
        [self.widthAnchor constraintEqualToConstant:width],
        [label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad],
        [label.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kHPad],
        [label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    ]];
    return self;
}

@end

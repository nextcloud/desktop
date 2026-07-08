/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncstaticinforow.h"

#import "trayaccountpopupmetrics.h"
#import "trayaccountpopupviewutils.h"

using namespace OCC::Mac::TrayPopupViewUtils;

@implementation NCStaticInfoRow

- (instancetype)initWithTitle:(NSString *)title icon:(NSImage *)icon width:(CGFloat)width
{
    self = [super init];
    if (!self) return nil;
    self.translatesAutoresizingMaskIntoConstraints = NO;

    auto label = [NSTextField labelWithString:title];
    label.font = [NSFont systemFontOfSize:13];
    label.textColor = NSColor.labelColor;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:label];

    auto constraints = [NSMutableArray arrayWithArray:@[
        [self.heightAnchor constraintEqualToConstant:kActionHeight],
        [self.widthAnchor constraintEqualToConstant:width],
        [label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [label.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kHPad],
    ]];

    if (icon) {
        auto iconView = [[[NSImageView alloc] init] autorelease];
        iconView.image = icon;
        iconView.contentTintColor = NSColor.secondaryLabelColor;
        iconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        iconView.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:iconView];
        [constraints addObjectsFromArray:@[
            [iconView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad],
            [iconView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [iconView.widthAnchor constraintEqualToConstant:kActivityPreviewIconSize],
            [iconView.heightAnchor constraintEqualToConstant:kActivityPreviewIconSize],
            [label.leadingAnchor constraintEqualToAnchor:iconView.trailingAnchor constant:8.0],
        ]];
    } else {
        [constraints addObject:[label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad]];
    }

    auto availableTextWidth = width - (kHPad * 2.0);
    if (icon) {
        availableTextWidth -= kActivityPreviewIconSize + 8.0;
    }
    if (labelLikelyClipsText(label, title, availableTextWidth)) {
        setSharedToolTip(title, @[self, label]);
    }

    [NSLayoutConstraint activateConstraints:constraints];
    return self;
}

@end

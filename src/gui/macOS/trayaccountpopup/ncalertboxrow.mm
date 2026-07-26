/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncalertboxrow.h"

#import "ncpointinghandbutton.h"
#import "trayaccountpopupviewutils.h"

#include <QCoreApplication>

using namespace OCC::Mac::TrayPopupViewUtils;

@implementation NCAlertBoxRow {
    dispatch_block_t _action;
    NCActionHoverBlock _hoverAction;
}

- (instancetype)initWithTitle:(NSString *)title action:(dispatch_block_t)action hoverAction:(NCActionHoverBlock)hoverAction
{
    self = [super init];
    if (!self) return nil;

    _action = [action copy];
    _hoverAction = [hoverAction copy];
    self.translatesAutoresizingMaskIntoConstraints = NO;

    auto label = [NSTextField labelWithString:title];
    label.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    label.textColor = NSColor.labelColor;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.maximumNumberOfLines = 2;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:label];

    auto resolveButton = [[[NCPointingHandButton alloc] init] autorelease];
    resolveButton.title = QCoreApplication::translate("TrayAccountPopup", "Resolve").toNSString();
    resolveButton.target = self;
    resolveButton.action = @selector(resolveButtonClicked:);
    resolveButton.bezelStyle = NSBezelStyleRounded;
    resolveButton.controlSize = NSControlSizeSmall;
    resolveButton.font = [NSFont systemFontOfSize:11 weight:NSFontWeightRegular];
    resolveButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:resolveButton];

    [NSLayoutConstraint activateConstraints:@[
        [self.widthAnchor constraintEqualToConstant:kPopupWidth],
        [self.heightAnchor constraintGreaterThanOrEqualToConstant:kActionHeight],

        [label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad + kAvatarSize + 10.0],
        [label.trailingAnchor constraintLessThanOrEqualToAnchor:resolveButton.leadingAnchor constant:-8.0],
        [label.topAnchor constraintEqualToAnchor:self.topAnchor constant:kAccountHoverVerticalMargin],
        [label.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-kAccountHoverVerticalMargin],

        [resolveButton.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-kHPad],
        [resolveButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [resolveButton.widthAnchor constraintGreaterThanOrEqualToConstant:76.0],
    ]];

    const auto availableTextWidth = kPopupWidth - (kHPad + kAvatarSize + 10.0) - kHPad - 76.0 - 8.0;
    if (labelLikelyClipsText(label, title, availableTextWidth)) {
        setSharedToolTip(title, @[self, label]);
    }

    return self;
}

- (void)dealloc
{
    [_action release];
    [_hoverAction release];
    [super dealloc];
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    auto trackingAreas = [self.trackingAreas copy];
    for (NSTrackingArea *area in trackingAreas) {
        [self removeTrackingArea:area];
    }
    [trackingAreas release];

    auto trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                     options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
                                                       owner:self
                                                    userInfo:nil];
    [self addTrackingArea:trackingArea];
    [trackingArea release];
}

- (void)mouseEntered:(NSEvent *)event
{
    if (_hoverAction) {
        _hoverAction(self);
    }
}

- (void)mouseUp:(NSEvent *)event
{
    if (_action) {
        _action();
    }
}

- (void)resolveButtonClicked:(id)sender
{
    if (_action) {
        _action();
    }
}

@end

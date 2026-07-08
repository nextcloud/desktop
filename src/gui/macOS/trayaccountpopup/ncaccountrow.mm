/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncaccountrow.h"

#import "trayaccountpopupmetrics.h"
#import "trayaccountpopupviewutils.h"

using namespace OCC::Mac::TrayPopupViewUtils;

@implementation NCAccountRow {
    NSView *_hoverView;
    BOOL _mouseInside;
    BOOL _persistentHighlight;
}

- (instancetype)init
{
    self = [super init];
    if (!self) return nil;
    self.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);

    _hoverView = [[[NSView alloc] init] autorelease];
    _hoverView.wantsLayer = YES;
    _hoverView.layer.backgroundColor = hoverColor().CGColor;
    _hoverView.layer.cornerRadius = kHoverRadius;
    _hoverView.hidden = YES;
    [self addSubview:_hoverView];
    return self;
}

- (void)updateHoverHighlight
{
    _hoverView.hidden = !(_mouseInside || _persistentHighlight);
}

- (void)setPersistentHighlight:(BOOL)persistentHighlight
{
    _persistentHighlight = persistentHighlight;
    [self updateHoverHighlight];
}

- (void)layout
{
    [super layout];
    _hoverView.frame = NSInsetRect(self.bounds, kHoverMargin, kAccountHoverVerticalMargin);
}

- (void)mouseEntered:(NSEvent *)event
{
    _mouseInside = YES;
    [self updateHoverHighlight];
    [self.popupDelegate onAccountRowHovered:self];
}

- (void)mouseExited:(NSEvent *)event
{
    _mouseInside = NO;
    [self updateHoverHighlight];
}

- (void)mouseUp:(NSEvent *)event
{
    [self.popupDelegate onAccountRowClicked:self.userIndex];
}

@end

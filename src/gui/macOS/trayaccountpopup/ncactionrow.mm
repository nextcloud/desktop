/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncactionrow.h"

#import "trayaccountpopupviewutils.h"

using namespace OCC::Mac::TrayPopupViewUtils;

@implementation NCActionRow {
    dispatch_block_t _action;
    NCActionHoverBlock _hoverAction;
    NSView *_hoverView;
    NSImageView *_iconView;
    NSTextField *_label;
    BOOL _actionEnabled;
    BOOL _iconTintedToLabelColor;
    BOOL _mouseInside;
    BOOL _persistentHighlight;
}

- (instancetype)initWithTitle:(NSString *)title action:(dispatch_block_t)action
{
    return [self initWithTitle:title width:kPopupWidth enabled:YES action:action];
}

- (instancetype)initWithTitle:(NSString *)title
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
{
    return [self initWithTitle:title icon:nil width:width enabled:enabled action:action hoverAction:nil];
}

- (instancetype)initWithTitle:(NSString *)title
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
{
    return [self initWithTitle:title icon:nil width:width enabled:enabled action:action hoverAction:hoverAction showsSubmenuIndicator:NO];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
{
    return [self initWithTitle:title icon:icon width:width enabled:enabled action:action hoverAction:nil];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
{
    return [self initWithTitle:title icon:icon width:width enabled:enabled action:action hoverAction:hoverAction showsSubmenuIndicator:NO];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
        showsSubmenuIndicator:(BOOL)showsSubmenuIndicator
{
    return [self initWithTitle:title
                          icon:icon
                      subtitle:nil
                      dateTime:nil
                         width:width
                       enabled:enabled
                        action:action
                   hoverAction:hoverAction
         showsSubmenuIndicator:showsSubmenuIndicator];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
{
    return [self initWithTitle:title
                          icon:icon
                      subtitle:nil
                      dateTime:dateTime
                         width:width
                       enabled:enabled
                        action:action
                   hoverAction:hoverAction
         showsSubmenuIndicator:NO];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
        showsSubmenuIndicator:(BOOL)showsSubmenuIndicator
{
    return [self initWithTitle:title
                          icon:icon
                      subtitle:nil
                      dateTime:dateTime
                         width:width
                       enabled:enabled
                        action:action
                   hoverAction:hoverAction
         showsSubmenuIndicator:showsSubmenuIndicator];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     subtitle:(NSString *)subtitle
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
{
    return [self initWithTitle:title
                          icon:icon
                      subtitle:subtitle
                      dateTime:dateTime
                         width:width
                       enabled:enabled
                        action:action
                   hoverAction:hoverAction
         showsSubmenuIndicator:NO];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     subtitle:(NSString *)subtitle
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
        showsSubmenuIndicator:(BOOL)showsSubmenuIndicator
{
    self = [super init];
    if (!self) return nil;
    _action = [action copy];
    _hoverAction = [hoverAction copy];
    _actionEnabled = enabled;
    self.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);

    _hoverView = [[[NSView alloc] init] autorelease];
    _hoverView.wantsLayer = YES;
    _hoverView.layer.backgroundColor = hoverColor().CGColor;
    _hoverView.layer.cornerRadius = kHoverRadius;
    _hoverView.hidden = YES;
    [self addSubview:_hoverView];

    _label = [NSTextField labelWithString:title];
    const auto isPreviewRow = dateTime.length > 0;
    const auto hasSubtitle = subtitle.length > 0;
    _label.font = isPreviewRow ? [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold] : [NSFont systemFontOfSize:13];
    _label.textColor = enabled ? NSColor.labelColor : NSColor.tertiaryLabelColor;
    _label.lineBreakMode = isPreviewRow && !hasSubtitle ? NSLineBreakByWordWrapping : NSLineBreakByTruncatingTail;
    _label.maximumNumberOfLines = isPreviewRow && !hasSubtitle ? 2 : 1;
    _label.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *textContainer = nil;
    NSTextField *subtitleLabel = nil;
    NSTextField *dateTimeLabel = nil;
    if (isPreviewRow) {
        textContainer = [[[NSView alloc] init] autorelease];
        textContainer.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:textContainer];
        [textContainer addSubview:_label];

        if (hasSubtitle) {
            subtitleLabel = [NSTextField labelWithString:subtitle];
            subtitleLabel.font = [NSFont systemFontOfSize:13];
            subtitleLabel.textColor = enabled ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;
            subtitleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
            subtitleLabel.maximumNumberOfLines = 1;
            subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
            [textContainer addSubview:subtitleLabel];
        }

        dateTimeLabel = [NSTextField labelWithString:dateTime];
        dateTimeLabel.font = [NSFont systemFontOfSize:11];
        dateTimeLabel.textColor = enabled ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;
        dateTimeLabel.alignment = NSTextAlignmentLeft;
        dateTimeLabel.lineBreakMode = NSLineBreakByTruncatingTail;
        dateTimeLabel.maximumNumberOfLines = 1;
        dateTimeLabel.translatesAutoresizingMaskIntoConstraints = NO;
        [textContainer addSubview:dateTimeLabel];
    } else {
        [self addSubview:_label];
    }

    auto constraints = [NSMutableArray arrayWithArray:@[
        [self.heightAnchor constraintEqualToConstant:isPreviewRow ? (hasSubtitle ? kDetailedPreviewActionHeight : kPreviewActionHeight) : kActionHeight],
        [self.widthAnchor constraintEqualToConstant:width],
    ]];

    if (isPreviewRow) {
        [constraints addObjectsFromArray:@[
            [textContainer.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [textContainer.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor constant:4.0],
            [textContainer.bottomAnchor constraintLessThanOrEqualToAnchor:self.bottomAnchor constant:-4.0],
            [_label.topAnchor constraintEqualToAnchor:textContainer.topAnchor],
            [_label.leadingAnchor constraintEqualToAnchor:textContainer.leadingAnchor],
            [_label.trailingAnchor constraintEqualToAnchor:textContainer.trailingAnchor],
            [dateTimeLabel.leadingAnchor constraintEqualToAnchor:textContainer.leadingAnchor],
            [dateTimeLabel.trailingAnchor constraintEqualToAnchor:textContainer.trailingAnchor],
            [dateTimeLabel.bottomAnchor constraintEqualToAnchor:textContainer.bottomAnchor],
        ]];
        if (hasSubtitle) {
            [constraints addObjectsFromArray:@[
                [subtitleLabel.topAnchor constraintEqualToAnchor:_label.bottomAnchor],
                [subtitleLabel.leadingAnchor constraintEqualToAnchor:textContainer.leadingAnchor],
                [subtitleLabel.trailingAnchor constraintEqualToAnchor:textContainer.trailingAnchor],
                [dateTimeLabel.topAnchor constraintEqualToAnchor:subtitleLabel.bottomAnchor],
            ]];
        } else {
            [constraints addObject:[dateTimeLabel.topAnchor constraintEqualToAnchor:_label.bottomAnchor]];
        }
    } else {
        [constraints addObject:[_label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor]];
    }

    if (showsSubmenuIndicator) {
        NSImageView *chevron = [[[NSImageView alloc] init] autorelease];
        chevron.image = [[NSImage imageWithSystemSymbolName:@"chevron.right" accessibilityDescription:nil]
            imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:11 weight:NSFontWeightMedium]];
        chevron.contentTintColor = enabled ? NSColor.tertiaryLabelColor : NSColor.quaternaryLabelColor;
        chevron.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:chevron];
        [constraints addObjectsFromArray:@[
            [chevron.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-kHPad],
            [chevron.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [chevron.widthAnchor constraintEqualToConstant:8],
            [chevron.heightAnchor constraintEqualToConstant:13],
        ]];
        if (isPreviewRow) {
            [constraints addObject:[textContainer.trailingAnchor constraintLessThanOrEqualToAnchor:chevron.leadingAnchor constant:-8]];
        } else {
            [constraints addObject:[_label.trailingAnchor constraintLessThanOrEqualToAnchor:chevron.leadingAnchor constant:-8]];
        }
    } else {
        if (isPreviewRow) {
            [constraints addObject:[textContainer.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kHPad]];
        } else {
            [constraints addObject:[_label.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kHPad]];
        }
    }

    if (icon) {
        _iconView = [[[NSImageView alloc] init] autorelease];
        _iconView.image = icon;
        _iconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _iconView.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:_iconView];
        [constraints addObjectsFromArray:@[
            [_iconView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad],
            [_iconView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [_iconView.widthAnchor constraintEqualToConstant:kActionIconSize],
            [_iconView.heightAnchor constraintEqualToConstant:kActionIconSize],
        ]];
        if (isPreviewRow) {
            [constraints addObject:[textContainer.leadingAnchor constraintEqualToAnchor:_iconView.trailingAnchor constant:8.0]];
        } else {
            [constraints addObject:[_label.leadingAnchor constraintEqualToAnchor:_iconView.trailingAnchor constant:8.0]];
        }
    } else {
        if (isPreviewRow) {
            [constraints addObject:[textContainer.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad]];
        } else {
            [constraints addObject:[_label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad]];
        }
    }

    auto availableTextWidth = width - (kHPad * 2.0);
    if (icon) {
        availableTextWidth -= kActionIconSize + 8.0;
    }
    if (showsSubmenuIndicator) {
        availableTextWidth -= 8.0 + 8.0;
    }

    auto needsToolTip = labelLikelyClipsText(_label, title, availableTextWidth);
    if (subtitleLabel) {
        needsToolTip = needsToolTip || labelLikelyClipsText(subtitleLabel, subtitle, availableTextWidth);
    }
    if (dateTimeLabel) {
        needsToolTip = needsToolTip || labelLikelyClipsText(dateTimeLabel, dateTime, availableTextWidth);
    }
    if (needsToolTip) {
        auto toolTipViews = [NSMutableArray arrayWithObjects:self, _label, nil];
        if (textContainer) {
            [toolTipViews addObject:textContainer];
        }
        if (subtitleLabel) {
            [toolTipViews addObject:subtitleLabel];
        }
        if (dateTimeLabel) {
            [toolTipViews addObject:dateTimeLabel];
        }
        setSharedToolTip(menuRowToolTipText(title, subtitle, dateTime), toolTipViews);
    }

    [NSLayoutConstraint activateConstraints:constraints];
    return self;
}

- (void)setIcon:(NSImage *)icon
{
    if (icon && _iconView) {
        if (_iconTintedToLabelColor) {
            auto templateIcon = [icon copy];
            [templateIcon setTemplate:YES];
            _iconView.contentTintColor = NSColor.labelColor;
            _iconView.image = templateIcon;
            [templateIcon release];
        } else {
            _iconView.contentTintColor = nil;
            _iconView.image = icon;
        }
    }
}

- (void)setIconTintedToLabelColor:(BOOL)tinted
{
    _iconTintedToLabelColor = tinted;
    if (_iconView.image) {
        [self setIcon:_iconView.image];
    }
}

- (void)updateHoverHighlight
{
    _hoverView.hidden = !_actionEnabled || !(_mouseInside || _persistentHighlight);
}

- (void)setPersistentHighlight:(BOOL)persistentHighlight
{
    _persistentHighlight = persistentHighlight;
    [self updateHoverHighlight];
}

- (void)layout
{
    [super layout];
    _hoverView.frame = NSInsetRect(self.bounds, kHoverMargin, 0.0);
}

- (void)mouseEntered:(NSEvent *)event
{
    _mouseInside = YES;
    [self updateHoverHighlight];
    if (_actionEnabled && _hoverAction) _hoverAction(self);
}

- (void)mouseExited:(NSEvent *)event
{
    _mouseInside = NO;
    [self updateHoverHighlight];
}

- (void)mouseUp:(NSEvent *)event
{
    if (_actionEnabled && _action) _action();
}

- (void)dealloc
{
    [_action release];
    [_hoverAction release];
    [super dealloc];
}

@end

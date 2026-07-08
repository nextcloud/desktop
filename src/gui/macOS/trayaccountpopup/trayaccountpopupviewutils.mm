/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "trayaccountpopupviewutils.h"

#include "trayaccountpopupmetrics.h"

namespace OCC::Mac::TrayPopupViewUtils {

static CGFloat clampedPopupOriginCoordinate(const CGFloat origin, const CGFloat minEdge, const CGFloat maxEdge, const CGFloat size)
{
    const auto minOrigin = minEdge + kScreenEdgePadding;
    const auto maxOrigin = maxEdge - size - kScreenEdgePadding;
    if (maxOrigin < minOrigin) {
        return minOrigin;
    }
    return origin < minOrigin ? minOrigin : (origin > maxOrigin ? maxOrigin : origin);
}

NSPoint clampedPopupOrigin(const NSPoint origin, const NSSize size, const NSRect visibleFrame)
{
    return NSMakePoint(clampedPopupOriginCoordinate(origin.x, NSMinX(visibleFrame), NSMaxX(visibleFrame), size.width),
                       clampedPopupOriginCoordinate(origin.y, NSMinY(visibleFrame), NSMaxY(visibleFrame), size.height));
}

void addOwnedArrangedSubview(NSStackView *stack, NSView *view)
{
    [stack addArrangedSubview:view];
    [view release];
}

static CGFloat textWidth(NSString *text, NSFont *font)
{
    if (text.length == 0 || !font) {
        return 0.0;
    }

    return [text sizeWithAttributes:@{ NSFontAttributeName : font }].width;
}

BOOL labelLikelyClipsText(NSTextField *label, NSString *text, const CGFloat availableWidth)
{
    if (!label || text.length == 0 || availableWidth <= 0.0) {
        return NO;
    }

    const auto maximumNumberOfLines = label.maximumNumberOfLines > 0 ? label.maximumNumberOfLines : 1;
    return textWidth(text, label.font) > availableWidth * maximumNumberOfLines;
}

NSString *menuRowToolTipText(NSString *title, NSString *subtitle, NSString *dateTime)
{
    auto parts = [NSMutableArray array];
    if (title.length > 0) {
        [parts addObject:title];
    }
    if (subtitle.length > 0) {
        [parts addObject:subtitle];
    }
    if (dateTime.length > 0) {
        [parts addObject:dateTime];
    }

    return parts.count > 0 ? [parts componentsJoinedByString:@"\n"] : nil;
}

void setSharedToolTip(NSString *toolTip, NSArray *views)
{
    if (toolTip.length == 0) {
        return;
    }

    for (NSView *view in views) {
        view.toolTip = toolTip;
    }
}

NSStackView *configurePopupPanel(NSPanel *panel)
{
    panel.level = NSPopUpMenuWindowLevel;
    panel.hasShadow = YES;
    panel.releasedWhenClosed = NO;
    panel.backgroundColor = NSColor.clearColor;
    panel.opaque = NO;

    auto container = [[[NSView alloc] init] autorelease];
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    panel.contentView = container;

    auto visualEffectView = [[[NSVisualEffectView alloc] init] autorelease];
    visualEffectView.material = NSVisualEffectMaterialHUDWindow;
    visualEffectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    visualEffectView.state = NSVisualEffectStateActive;
    visualEffectView.wantsLayer = YES;
    visualEffectView.layer.cornerRadius = kCornerRadius;
    visualEffectView.layer.masksToBounds = YES;
    visualEffectView.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:visualEffectView];

    auto stack = [NSStackView stackViewWithViews:@[]];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.spacing = 0;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [visualEffectView addSubview:stack];

    [NSLayoutConstraint activateConstraints:@[
        [visualEffectView.topAnchor constraintEqualToAnchor:container.topAnchor],
        [visualEffectView.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [visualEffectView.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
        [visualEffectView.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
        [stack.topAnchor constraintEqualToAnchor:visualEffectView.topAnchor],
        [stack.leadingAnchor constraintEqualToAnchor:visualEffectView.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:visualEffectView.trailingAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:visualEffectView.bottomAnchor],
    ]];

    return stack;
}

void clearStack(NSStackView *stack)
{
    auto arrangedSubviews = [stack.arrangedSubviews copy];
    for (NSView *view in arrangedSubviews) {
        [stack removeArrangedSubview:view];
        [view removeFromSuperview];
    }
    [arrangedSubviews release];
}

void positionPopupFromRow(NSPanel *popup, NSView *row)
{
    const auto rowTopLeftInWindow = [row convertPoint:NSMakePoint(NSMinX(row.bounds) + kHPad, NSMaxY(row.bounds)) toView:nil];
    const auto rowTopRightInWindow = [row convertPoint:NSMakePoint(NSMaxX(row.bounds) - kHPad, NSMaxY(row.bounds)) toView:nil];
    const auto rowTopLeftOnScreen = [row.window convertPointToScreen:rowTopLeftInWindow];
    auto rowTopRightOnScreen = [row.window convertPointToScreen:rowTopRightInWindow];

    const auto popupWidth = popup.frame.size.width;
    const auto popupHeight = popup.frame.size.height;
    auto popupOrigin = rowTopRightOnScreen;
    popupOrigin.y -= popupHeight;

    auto screen = row.window.screen;
    if (!screen) {
        screen = NSScreen.mainScreen ?: NSScreen.screens.firstObject;
    }
    const auto visibleFrame = screen.visibleFrame;
    const auto rightEdge = NSMaxX(visibleFrame) - kScreenEdgePadding;
    const auto leftEdge = NSMinX(visibleFrame) + kScreenEdgePadding;

    if (popupOrigin.x + popupWidth > rightEdge && rowTopLeftOnScreen.x - popupWidth >= leftEdge) {
        popupOrigin.x = rowTopLeftOnScreen.x - popupWidth;
    }
    popupOrigin = clampedPopupOrigin(popupOrigin, NSMakeSize(popupWidth, popupHeight), visibleFrame);

    [popup setFrameOrigin:popupOrigin];
}

NSColor *hoverColor()
{
    NSAppearanceName appearanceName = [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua,
        NSAppearanceNameDarkAqua,
    ]];
    const BOOL isDarkMode = [appearanceName isEqualToString:NSAppearanceNameDarkAqua];
    return [(isDarkMode ? NSColor.whiteColor : NSColor.blackColor) colorWithAlphaComponent:0.08];
}

}

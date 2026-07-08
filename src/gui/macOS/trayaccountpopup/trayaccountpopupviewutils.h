/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

namespace OCC::Mac::TrayPopupViewUtils {

// Build the blurred, rounded panel chrome and return the vertical stack view the
// caller fills with rows.
NSStackView *configurePopupPanel(NSPanel *panel);

// Remove and release every arranged subview from a stack.
void clearStack(NSStackView *stack);

// Add a freshly-allocated view to the stack and release the caller's +1 reference.
void addOwnedArrangedSubview(NSStackView *stack, NSView *view);

// The subtle row highlight colour for the current (light/dark) appearance.
NSColor *hoverColor();

// Clamp a popup origin toward the screen's visible frame, inset by kScreenEdgePadding.
// A popup larger than the visible frame can still overflow its far edge.
NSPoint clampedPopupOrigin(const NSPoint origin, const NSSize size, const NSRect visibleFrame);

// Position a submenu popup next to the given row, flipping and clamping to screen.
void positionPopupFromRow(NSPanel *popup, NSView *row);

// Heuristic: would the label's text be clipped in availableWidth given its font
// and maximum line count?
BOOL labelLikelyClipsText(NSTextField *label, NSString *text, const CGFloat availableWidth);

// Join the non-empty title/subtitle/date-time into a multi-line tool tip string.
NSString *menuRowToolTipText(NSString *title, NSString *subtitle, NSString *dateTime);

// Apply the same tool tip to every view in the array (no-op for an empty tip).
void setSharedToolTip(NSString *toolTip, NSArray *views);

}

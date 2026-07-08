/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

// Shared, lightweight declarations for the macOS tray account popup.
//
// These are intentionally kept together in a single header rather than split
// per identifier: they are plain layout constants and a single block alias, not
// stand-alone types. Every value below has internal linkage (a `constexpr` at
// namespace scope is implicitly `const`), so including this header in multiple
// translation units is free of ODR conflicts.

// Layout metrics shared across the popup views. Keep behavior and menu taxonomy
// aligned with src/gui/trayaccountpopup_qt.cpp.
constexpr CGFloat kPopupWidth = 300.0;
constexpr CGFloat kRowHeight = 48.0;
constexpr CGFloat kAvatarSize = 34.0;
constexpr CGFloat kTopPadding = 4.0;
constexpr CGFloat kActionHeight = 26.0;
constexpr CGFloat kActionIconSize = 18.0;
constexpr CGFloat kPreviewActionHeight = 52.0;
constexpr CGFloat kDetailedPreviewActionHeight = 58.0;
constexpr CGFloat kActionVerticalPadding = 8.0;
constexpr CGFloat kCornerRadius = 14.0;
constexpr CGFloat kHPad = 14.0;
constexpr CGFloat kScreenEdgePadding = 8.0;
constexpr CGFloat kStatusItemLeadingOffset = 3.0;
constexpr CGFloat kHoverMargin = 5.0;
constexpr CGFloat kHoverRadius = 5.0;
constexpr CGFloat kAccountHoverVerticalMargin = 4.0;
constexpr CGFloat kCompactSeparatorVerticalMargin = 2.0;
constexpr CGFloat kAccountActionsPopupWidth = 340.0;
constexpr CGFloat kAppsPopupWidth = 220.0;
constexpr CGFloat kNotificationActionsPopupWidth = 160.0;
constexpr CGFloat kSectionHeaderHeight = 24.0;
constexpr CGFloat kActivityPreviewIconSize = 16.0;

// Block invoked when the pointer enters an interactive row, passing the hovered
// row so the owning popup can react (e.g. reveal or hide a submenu).
typedef void (^NCActionHoverBlock)(NSView *row);

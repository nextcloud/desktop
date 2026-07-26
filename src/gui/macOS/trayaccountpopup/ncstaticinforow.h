/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

/**
 * @brief A non-interactive informational row with a title and optional leading icon.
 *
 * Used for placeholders such as a "No recent activity" entry.
 */
@interface NCStaticInfoRow : NSView
/**
 * @brief Creates an informational row.
 * @param icon Optional leading icon; pass nil for none.
 * @param width Fixed width of the row.
 */
- (instancetype)initWithTitle:(NSString *)title icon:(NSImage *)icon width:(CGFloat)width;
@end

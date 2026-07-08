/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

/**
 * @brief A non-interactive section header showing a small, secondary-colour title.
 */
@interface NCSectionHeaderRow : NSView
/** @brief Creates a header row with the given title and fixed @c width. */
- (instancetype)initWithTitle:(NSString *)title width:(CGFloat)width;
@end

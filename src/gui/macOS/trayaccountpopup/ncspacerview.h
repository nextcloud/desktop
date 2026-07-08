/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

/**
 * @brief A fixed-size empty view used to add spacing between rows.
 */
@interface NCSpacerView : NSView
/** @brief Creates a spacer of the given @c height and the default popup width. */
- (instancetype)initWithHeight:(CGFloat)height;
/** @brief Creates a spacer of the given @c height and @c width. */
- (instancetype)initWithHeight:(CGFloat)height width:(CGFloat)width;
@end

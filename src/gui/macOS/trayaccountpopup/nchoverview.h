/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

/**
 * @brief Base view that highlights itself while the pointer is over it.
 *
 * Installs a mouse enter/exit tracking area covering its bounds and, by
 * default, tints its layer background while hovered. Subclasses inherit the
 * tracking-area setup and may override the enter/exit handlers to draw their
 * own highlight instead.
 */
@interface NCHoverView : NSView
@end

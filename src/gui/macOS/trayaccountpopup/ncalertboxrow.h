/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

#import "trayaccountpopupmetrics.h"

/**
 * @brief A row showing an account alert message alongside a "Resolve" button.
 */
@interface NCAlertBoxRow : NSView
/**
 * @brief Creates an alert row.
 * @param title The alert message.
 * @param action Block run when the row or its "Resolve" button is clicked.
 * @param hoverAction Block run when the pointer enters the row, receiving the row itself.
 */
- (instancetype)initWithTitle:(NSString *)title action:(dispatch_block_t)action hoverAction:(NCActionHoverBlock)hoverAction;
@end

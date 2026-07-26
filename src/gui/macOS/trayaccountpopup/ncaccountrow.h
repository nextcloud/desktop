/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import "nchoverview.h"

@class NCAccountRow;

/**
 * @brief Receives click and hover callbacks from an NCAccountRow.
 */
@protocol NCAccountRowDelegate
/** @brief Sent when the row is clicked, passing the row's user index. */
- (void)onAccountRowClicked:(int)index;
/** @brief Sent when the pointer enters the row. */
- (void)onAccountRowHovered:(NCAccountRow *)row;
@end

/**
 * @brief A tray-popup row representing a single account.
 */
@interface NCAccountRow : NCHoverView
/** @brief Index of the account this row represents in the UserModel. */
@property (nonatomic, assign) int userIndex;
/** @brief Delegate notified when the row is clicked or hovered. */
@property (nonatomic, assign) id<NCAccountRowDelegate> popupDelegate;
/** @brief Keeps the row highlighted even when not hovered (e.g. while its submenu is open). */
- (void)setPersistentHighlight:(BOOL)persistentHighlight;
@end

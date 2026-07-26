/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

@class NCTrayPopup;

/**
 * @brief A borderless popup listing the apps available for an account.
 *
 * Selecting an app opens its URL and closes the whole tray popup.
 */
@interface NCAppsPopup : NSPanel
/**
 * @brief Rebuilds the app list for the given account (user) index.
 * @param owner The root tray popup, used to close all popups when an app is opened.
 */
- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner;
@end

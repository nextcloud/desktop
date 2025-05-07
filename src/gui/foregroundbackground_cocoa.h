/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <AppKit/NSApplication.h>

/**
* @brief CocoaProcessType provides methods for moving the application between
* the background and foreground.
* @ingroup gui
*/

#ifndef COCOAPROCESSTYPE_H
#define COCOAPROCESSTYPE_H

@interface CocoaProcessType : NSApplication

/**
 * @brief CocoaProcessTypeToForeground() enables the macOS menubar and dock icon, which are necessary for a maximized window to be able to exit full screen.
 * @ingroup gui
 */
+ (void)ToForeground;

/**
 * @brief CocoaProcessTypeToBackground() disables the macOS menubar and dock icon, so that the application will only be present as a menubar icon.
 * @ingroup gui
 */
+ (void)ToBackground;

@end

#endif
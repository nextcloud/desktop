/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MainAppServiceProtocol_h
#define MainAppServiceProtocol_h

#import <Foundation/Foundation.h>

@protocol MainAppServiceProtocol

/**
 * @brief Report the current status of the file provider extension.
 */
- (void)reportStatus:(NSString *)status;

@end

#endif

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AppProtocol_h
#define AppProtocol_h

#import <Foundation/Foundation.h>

/**
 * @brief The main app APIs exposed through XPC.
 */
@protocol AppProtocol

/**
 * @brief The file provider extension can report its synchronization status as a string constant value to the main app through this method.
 * @param status The synchronization status string.
 * @param domainIdentifier The file provider domain identifier for which the status is reported.
 */
- (void)reportSyncStatus:(NSString *)status forDomainIdentifier:(NSString *)domainIdentifier;

@end

#endif /* AppProtocol_h */

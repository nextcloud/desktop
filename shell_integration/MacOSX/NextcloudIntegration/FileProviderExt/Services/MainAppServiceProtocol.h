/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MainAppServiceProtocol_h
#define MainAppServiceProtocol_h

#import <FileProvider/FileProvider.h>
#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>

@protocol MainAppServiceProtocol

/**
 * @brief Report the current status of the file provider extension.
 * @param domainIdentifier Identifier of the File Provider domain that reports the status.
 * @param status Plain status string to log or process.
 */
- (void)reportStatusForDomain:(NSFileProviderDomainIdentifier)domainIdentifier status:(NSString *)status;

@end

#endif

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Foundation/Foundation.h>
#import <ServiceManagement/SMAppService.h>

@interface MacUtilityMock : NSObject

@property SMAppServiceStatus configuredStatus;
@property SMAppServiceStatus updatedStatus;
@property BOOL operationSucceeds;
@property BOOL includesError;
@property (copy) NSString *style;
@property (readonly) NSUInteger factoryCount;
@property (readonly) NSUInteger statusReadCount;
@property (readonly) NSUInteger registerCount;
@property (readonly) NSUInteger unregisterCount;
@property (readonly) NSUInteger preferenceReadCount;
@property (readonly) BOOL errorOutputProvided;
@property (readonly) NSInteger liveServiceCount;
@property (readonly) NSInteger liveErrorCount;
@property (readonly) NSInteger liveStyleCount;

- (BOOL)install;
- (void)uninstall;

@end

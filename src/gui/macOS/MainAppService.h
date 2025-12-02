#if 0
/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#endif

#pragma once

#import <Foundation/Foundation.h>

#import "MainAppServiceProtocol.h"

#ifdef __OBJC__
@interface MainAppService : NSObject <MainAppServiceProtocol>
+ (instancetype)sharedInstance;
@end
#endif
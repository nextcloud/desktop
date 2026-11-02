/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import "gui/macOS/trayaccountpopup/ncaccountrow.h"

@interface NCAccountRowDelegateMock : NSObject <NCAccountRowDelegate>

@property (nonatomic, readonly) int clickedIndex;
@property (nonatomic, readonly) NSUInteger clickCount;
@property (nonatomic, readonly) NSUInteger hoverCount;
@property (nonatomic, readonly, weak) NCAccountRow *hoveredRow;

@end

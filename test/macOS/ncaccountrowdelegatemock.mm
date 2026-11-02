/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncaccountrowdelegatemock.h"

@implementation NCAccountRowDelegateMock

- (void)onAccountRowClicked:(int)index
{
    _clickedIndex = index;
    ++_clickCount;
}

- (void)onAccountRowHovered:(NCAccountRow *)row
{
    _hoveredRow = row;
    ++_hoverCount;
}

@end

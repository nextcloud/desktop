/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Cocoa/Cocoa.h>

namespace OCC::Mac::TrayPopupViewTestUtils
{

void addOwnedView(NSStackView *stack, NSHashTable<NSView *> *views);
NSEvent *mouseEvent(NSEventType type);

}

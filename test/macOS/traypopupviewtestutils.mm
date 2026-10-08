/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "traypopupviewtestutils.h"

#include "gui/macOS/trayaccountpopup/trayaccountpopupviewutils.h"

static_assert(__has_feature(objc_arc), "The tray caller test requires ARC.");

namespace OCC::Mac::TrayPopupViewTestUtils
{

void addOwnedView(NSStackView *stack, NSHashTable<NSView *> *views)
{
    const auto view = [[NSView alloc] init];
    [views addObject:view];
    TrayPopupViewUtils::addOwnedArrangedSubview(stack, view);
}

NSEvent *mouseEvent(NSEventType type)
{
    if (type == NSEventTypeMouseEntered || type == NSEventTypeMouseExited) {
        return [NSEvent enterExitEventWithType:type
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                   eventNumber:0
                                trackingNumber:0
                                      userData:nullptr];
    }
    return [NSEvent mouseEventWithType:type location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:1 pressure:0];
}

}

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "macsandboxpanelutils.h"

#include <QWindow>

static_assert(__has_feature(objc_arc), "macsandboxpanelutils requires ARC.");

namespace OCC::Mac::SandboxPanelUtils
{

void beginSheet(NSSavePanel *panel, QWindow *parentWindow, void (^completionHandler)(NSModalResponse))
{
    // Use AppKit's asynchronous panel API instead of runModal(). Qt's Cocoa
    // event dispatcher can dismiss a nested native modal loop immediately.
    // Activating the parent first keeps the sheet on it while another app is in front.
    NSWindow *sheetParent = nil;
    if (parentWindow) {
        parentWindow->requestActivate();
        sheetParent = ((__bridge NSView *)reinterpret_cast<void *>(parentWindow->winId())).window;
    }
    if (!sheetParent) {
        sheetParent = NSApp.keyWindow ?: NSApp.mainWindow;
    }

    if (sheetParent) {
        [panel beginSheetModalForWindow:sheetParent completionHandler:completionHandler];
    } else {
        [panel beginWithCompletionHandler:completionHandler];
    }
}

} // namespace OCC::Mac::SandboxPanelUtils

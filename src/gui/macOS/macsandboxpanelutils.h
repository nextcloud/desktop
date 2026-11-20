/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <AppKit/AppKit.h>

class QWindow;

namespace OCC::Mac::SandboxPanelUtils
{

/**
 * Show an open or save panel as a sheet of parentWindow, which is activated first.
 *
 * Without parentWindow, or when it has no native window, the panel is attached to
 * the key or main window. When there is none either, the panel opens on its own.
 */
void beginSheet(NSSavePanel *panel, QWindow *parentWindow, void (^completionHandler)(NSModalResponse));

} // namespace OCC::Mac::SandboxPanelUtils

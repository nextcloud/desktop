/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nativetitlebar_mac.h"

#include <QColor>
#include <QQuickWindow>
#include <QWindow>

#import <AppKit/AppKit.h>

namespace OCC {

void styleNativeTitleBar(QWindow *window, bool hideTitleText)
{
    if (!window) {
        return;
    }

    const auto viewId = window->winId();
    if (!viewId) {
        return;
    }

    auto *const view = reinterpret_cast<NSView *>(viewId);
    NSWindow *const nsWindow = view.window;
    if (!nsWindow) {
        return;
    }

    // Keep the native (draggable) title bar, but let it blend into the window: transparent
    // background and no separator hairline. The content stays below the title bar (no full-size
    // content view), so macOS keeps handling window dragging itself.
    nsWindow.titlebarAppearsTransparent = YES;
    nsWindow.titleVisibility = hideTitleText ? NSWindowTitleHidden : NSWindowTitleVisible;
    nsWindow.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;

    // Only a QQuickWindow exposes a flat fill colour we must mirror; widget windows already carry a
    // matching NSWindow background. Interpret it in the display's colour space rather than sRGB:
    // Qt composites the content without colour management, so an sRGB NSColor would be re-mapped
    // through the display profile and drift from the body on wide-gamut screens.
    if (auto *const quickWindow = qobject_cast<QQuickWindow *>(window)) {
        const QColor color = quickWindow->color();
        if (color.isValid()) {
            NSColorSpace *const colorSpace = nsWindow.screen.colorSpace ?: NSColorSpace.sRGBColorSpace;
            const CGFloat components[] = {
                static_cast<CGFloat>(color.redF()),
                static_cast<CGFloat>(color.greenF()),
                static_cast<CGFloat>(color.blueF()),
                static_cast<CGFloat>(color.alphaF()),
            };
            nsWindow.backgroundColor = [NSColor colorWithColorSpace:colorSpace components:components count:4];
        }
    }
}

}

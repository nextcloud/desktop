/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NATIVETITLEBAR_MAC_H
#define NATIVETITLEBAR_MAC_H

class QWindow;

namespace OCC {

/**
 * @brief Style a window's native macOS title bar so it blends into the window content.
 *
 * Keeps the native (draggable) title bar but makes its background transparent and removes the
 * separator hairline. For QQuickWindows the NSWindow background is matched to the window's colour
 * (in the display's colour space) so the colour flows seamlessly through the title bar.
 *
 * Safe to call repeatedly — e.g. from a colorChanged signal or a changeEvent — so it survives macOS
 * light/dark switches, which otherwise reset the styling.
 *
 * @param window         the top-level window to style (a QQuickWindow, or a QWidget's windowHandle()).
 * @param hideTitleText  true to hide the title text (account wizard); false to keep it (settings).
 * @ingroup gui
 */
void styleNativeTitleBar(QWindow *window, bool hideTitleText);

}

#endif

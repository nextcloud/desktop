/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QMLSCREENSHOTCAPTUREUTILS_H
#define QMLSCREENSHOTCAPTUREUTILS_H

class QQuickWindow;

namespace OCC::UiScreenshots {

/** @brief Applies screenshot-only icon, event-filter, positioning, and window flags around production QML. */
void configureQuickWindow(QQuickWindow &window, bool wizardWindow);

/** @brief Applies the existing production native title-bar styling to a wizard capture. */
void styleWizardTitleBar(QQuickWindow &window);

}

#endif // QMLSCREENSHOTCAPTUREUTILS_H

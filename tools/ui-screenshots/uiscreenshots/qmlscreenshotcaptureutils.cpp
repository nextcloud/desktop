/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qmlscreenshotcaptureutils.h"

#include "foregroundbackground_interface.h"
#include "nativetitlebar_mac.h"
#include "theme.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>

namespace OCC::UiScreenshots {

void configureQuickWindow(QQuickWindow &window, const bool wizardWindow)
{
    window.setIcon(Theme::instance()->applicationIcon());

    auto *foregroundBackground = new ForegroundBackground(&window);
    window.installEventFilter(foregroundBackground);

    if (!wizardWindow) {
        window.setFlag(Qt::ExpandedClientAreaHint, true);
        window.setFlag(Qt::NoTitleBarBackgroundHint, true);
    }

    if (const auto *screen = QGuiApplication::primaryScreen()) {
        window.setPosition(screen->availableGeometry().center() - QPoint(window.width() / 2, window.height() / 2));
    }
}

void styleWizardTitleBar(QQuickWindow &window)
{
    styleNativeTitleBar(&window, true);
    QObject::connect(&window, &QQuickWindow::colorChanged, &window, [&window] {
        styleNativeTitleBar(&window, true);
    });
}

}

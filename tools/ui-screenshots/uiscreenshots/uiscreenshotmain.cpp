/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cocoainitializer.h"
#include "uiscreenshotrunner.h"

#include <QQuickStyle>
#include <QQuickWindow>
#include <QResource>
#include <QSurfaceFormat>

#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(theme);

    OCC::Mac::CocoaInitializer cocoaInitializer;

    auto surfaceFormat = QSurfaceFormat::defaultFormat();
    surfaceFormat.setOption(QSurfaceFormat::ResetNotification);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    QQuickStyle::setStyle(QStringLiteral("macOS"));

    const auto exitCode = OCC::UiScreenshots::runIfRequested(argc, argv, {});
    if (!exitCode) {
        std::fputs("Nextcloud UI Screenshots: NEXTCLOUD_UI_SCREENSHOTS must select qml or native.\n", stderr);
        return EXIT_FAILURE;
    }
    return *exitCode;
}

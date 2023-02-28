/*
 * Copyright (C) by Erik Verbruggen <erik@verbruggen.consulting>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "platform_win.h"

#include <QApplication>
#include <QGuiApplication>

namespace OCC {

WinPlatform::WinPlatform()
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton, true);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

WinPlatform::~WinPlatform()
{
}

void WinPlatform::setApplication(QCoreApplication *application)
{
    // Ensure OpenSSL config file is only loaded from app directory
    const QString opensslConf = QCoreApplication::applicationDirPath() + QStringLiteral("/openssl.cnf");
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());

    // The Windows style still has pixelated elements with Qt 5.6,
    // it's recommended to use the Fusion style in this case, even
    // though it looks slightly less native. Check here after the
    // QApplication was constructed, but before any QWidget is
    // constructed.
    if (auto guiApp = qobject_cast<QGuiApplication *>(application)) {
        if (guiApp->devicePixelRatio() > 1) {
            QApplication::setStyle(QStringLiteral("fusion"));
        }
    }
}

} // namespace OCC

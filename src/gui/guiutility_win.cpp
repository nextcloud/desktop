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

#include "application.h"
#include "guiutility.h"

#include <QCoreApplication>

namespace OCC {

void Utility::tweakUIStyle()
{
    // The Windows style still has pixelated elements with Qt 5.6,
    // it's recommended to use the Fusion style in this case, even
    // though it looks slightly less native. Check here after the
    // QApplication was constructed, but before any QWidget is
    // constructed.
    if (qGuiApp->devicePixelRatio() > 1) {
        QApplication::setStyle(QStringLiteral("fusion"));
    }
}

void Utility::startShellIntegration()
{
}

QString Utility::socketApiSocketPath()
{
    return QLatin1String("\\\\.\\pipe\\") + QLatin1String("ownCloud-") + qEnvironmentVariable("USERNAME");
    // TODO: once the windows extension supports multiple
    // client connections, switch back to the theme name
    // See issue #2388
    // + Theme::instance()->appName();
}

} // namespace OCC

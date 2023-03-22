/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "resources.h"

#include <QPalette>

using namespace OCC;
using namespace Resources;

bool OCC::Resources::isUsingDarkTheme()
{
    // TODO: replace by a command line switch
    static bool forceDark = qEnvironmentVariableIntValue("OWNCLOUD_FORCE_DARK_MODE") != 0;
    return forceDark || QPalette().base().color().lightnessF() <= 0.5;
}

QIcon OCC::Resources::getCoreIcon(const QString &icon_name)
{
    if (icon_name.isEmpty()) {
        return {};
    }
    const QString theme = Resources::isUsingDarkTheme() ? QStringLiteral("dark") : QStringLiteral("light");
    const QString path = QStringLiteral(":/client/resources/%1/%2").arg(theme, icon_name);
    const QIcon icon(path);
    // were we able to load the file?
    Q_ASSERT(icon.actualSize({100, 100}).isValid());
    return icon;
}

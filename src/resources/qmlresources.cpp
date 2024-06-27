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

#include "resources/qmlresources.h"

#include "resources/resources.h"

using namespace OCC;
QUrl Resources::QMLResources::resourcePath(const QString &theme, const QString &icon, bool enabled)
{
    auto getBool = [](bool b) { return b ? QStringLiteral("true") : QStringLiteral("false"); };
    const auto data = QStringLiteral("{\"theme\": \"%1\", \"enabled\": %2, \"icon\": \"%3\", \"systemtheme\": %4}")
                          .arg(theme, getBool(enabled), icon, getBool(Resources::isUsingDarkTheme()));
    return QUrl(QStringLiteral("image://ownCloud/") + QString::fromUtf8(data.toUtf8().toBase64()));
}

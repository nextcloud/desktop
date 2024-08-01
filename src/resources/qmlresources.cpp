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
QUrl Resources::QMLResources::resourcePath2(const QString &provider, const QString &icon, bool enabled, const QVariantMap &properies)
{
    auto map =
        QVariantMap{{QStringLiteral("enabled"), enabled}, {QStringLiteral("icon"), icon}, {QStringLiteral("systemtheme"), Resources::isUsingDarkTheme()}};
    map.insert(properies);
    const auto data = QJsonDocument::fromVariant(map).toJson();
    return QUrl(QStringLiteral("image://%1/%2").arg(provider, QString::fromUtf8(data.toBase64())));
}

QUrl Resources::QMLResources::resourcePath(const QString &theme, const QString &icon, bool enabled)
{
    return resourcePath2(QStringLiteral("ownCloud"), icon, enabled, {{QStringLiteral("theme"), theme}});
}

Resources::QMLResources::Icon Resources::QMLResources::parseIcon(const QString &id)
{
    const auto data = QJsonDocument::fromJson(QByteArray::fromBase64(id.toUtf8())).object();
    return Icon{data.value(QLatin1String("theme")).toString(), data.value(QLatin1String("icon")).toString(), data.value(QLatin1String("enabled")).toBool()};
}

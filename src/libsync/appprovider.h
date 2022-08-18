/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#pragma once

#include "accountfwd.h"
#include "capabilities.h"
#include "owncloudlib.h"

#include <QJsonObject>
#include <QMimeType>
#include <QObject>
#include <QUrl>
#include <QVariantMap>

namespace OCC {
class OWNCLOUDSYNC_EXPORT AppProvider
{
public:
    struct OWNCLOUDSYNC_EXPORT Provider
    {
        // the server might provide multiple apps but no default
        // for now we only support default apps
        Provider() = default;
        explicit Provider(const QJsonObject &provider);
        QString mimeType;
        QString extension;
        QString name;
        QString description;
        QUrl icon;
        QString defaultApplication;
        bool allowCreation = false;

        bool isValid() const;
    };

    explicit AppProvider(const QJsonObject &apps = {});

    const Provider &app(const QMimeType &mimeType) const;
    const Provider &app(const QString &localPath) const;

    bool open(const AccountPtr &account, const QString &localPath, const QByteArray &fileId) const;


private:
    QHash<QString, Provider> _providers;
};

}

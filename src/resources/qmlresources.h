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

#pragma once
#include "resources/owncloudresources.h"

#include <QtQml/QtQml>

namespace OCC {
namespace Resources {

    class OWNCLOUDRESOURCES_EXPORT QMLResources : public QObject
    {
        Q_OBJECT
        QML_SINGLETON
        QML_ELEMENT
    public:
        using QObject::QObject;
        Q_INVOKABLE static QUrl resourcePath(const QString &theme, const QString &icon, bool enabled);
    };

} // Resources
} // OCC

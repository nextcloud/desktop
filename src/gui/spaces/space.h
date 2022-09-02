/*
 * Copyright (C) by Fabian Müller <fmueller@owncloud.com>
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

#include "OAIDrive.h"

#include <QPixmap>
#include <QString>
#include <QUrl>

namespace OCC::Spaces {

class Space
{
public:
    Space(const QString &name, const QString &subtitle, const QUrl &webUrl, const QUrl &webDavUrl, const QUrl &imageUrl);

    static Space fromDrive(const OpenAPI::OAIDrive &drive);

    [[nodiscard]] const QString &title() const;
    [[nodiscard]] const QString &subtitle() const;
    [[nodiscard]] const QUrl &webUrl() const;
    [[nodiscard]] const QUrl &webDavUrl() const;
    [[nodiscard]] const QUrl &imageUrl() const;

private:
    QString _name;
    QString _subtitle;
    QUrl _webUrl;
    QUrl _webDavUrl;
    QUrl _imageUrl;
};

} // OCC::Spaces

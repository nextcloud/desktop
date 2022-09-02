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

#include "space.h"
#include "gui/guiutility.h"

#include <QCoreApplication>
#include <QIcon>

namespace {
static const char contextC[] = "space";
const auto personalC = QLatin1String("personal");
const auto shareC = QLatin1String("virtual");
}

namespace OCC::Spaces {

Space::Space(const QString &name, const QString &subtitle, const QUrl &webUrl, const QUrl &webDavUrl, const QUrl &imageUrl)
    : _name(name)
    , _subtitle(subtitle)
    , _webUrl(webUrl)
    , _webDavUrl(webDavUrl)
    , _imageUrl(imageUrl)
{
}

const QString &Space::title() const
{
    return _name;
}

const QString &Space::subtitle() const
{
    return _subtitle;
}

const QUrl &Space::webUrl() const
{
    return _webDavUrl;
}

const QUrl &Space::webDavUrl() const
{
    return _webDavUrl;
}

const QUrl &Space::imageUrl() const
{
    return _imageUrl;
}

Space Space::fromDrive(const OpenAPI::OAIDrive &drive)
{
    QUrl imageUrl = [special = drive.getSpecial()] {
        const auto imageFolder = std::find_if(special.begin(), special.end(), [](const OpenAPI::OAIDriveItem &item) {
            return item.getSpecialFolder().getName() == QStringLiteral("image");
        });

        if (imageFolder == special.end()) {
            return QUrl();
        }

        return QUrl(imageFolder->getWebDavUrl());
    }();

    const QString driveTitle = [&drive]() {
        if (drive.getDriveType() == personalC) {
            return QCoreApplication::translate(contextC, "Personal");
        } else if (drive.getDriveType() == shareC) {
            // don't call it ShareJail
            return QCoreApplication::translate(contextC, "Shares");
        }
        return drive.getName();
    }();

    const QString driveSubtitle = [&drive]() {
        QString description = drive.getDescription();

        if (description.isEmpty() && drive.getDriveType() == personalC) {
            description = QCoreApplication::translate(contextC, "This is your personal space.");
        }

        return description;
    }();

    return {
        driveTitle,
        driveSubtitle,
        QUrl(drive.getWebUrl()),
        QUrl(drive.getRoot().getWebDavUrl()),
        imageUrl
    };
}

} // OCC::Spaces

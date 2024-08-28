/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "owncloudtheme.h"

#include <QString>
#include <QVariant>
#include <QPixmap>
#include <QIcon>
#include <QCoreApplication>

namespace OCC {

ownCloudTheme::ownCloudTheme()
    : Theme()
{
}

QColor ownCloudTheme::wizardHeaderBackgroundColor() const
{
    return QColor(4, 30, 66);
}

QColor ownCloudTheme::wizardHeaderTitleColor() const
{
    return Qt::white;
}

QIcon ownCloudTheme::wizardHeaderLogo() const
{
    return Resources::themeUniversalIcon(QStringLiteral("wizard_logo"));
}

QIcon ownCloudTheme::aboutIcon() const
{
    return Resources::themeUniversalIcon(QStringLiteral("oc-image-about"));
}

QmlButtonColor ownCloudTheme::primaryButtonColor() const
{
    const QColor button("#709cd2");
    return {button, Qt::white, button.darker()};
}

QmlButtonColor ownCloudTheme::secondaryButtonColor() const
{
    return {"#d4d3d0", Qt::black, QColor(Qt::black).lighter()};
}
}

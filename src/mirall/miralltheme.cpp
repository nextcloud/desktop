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

#include <QString>
#include <QDebug>
#include <QPixmap>

#include "miralltheme.h"

namespace Mirall {

mirallTheme::mirallTheme()
{
    qDebug() << " ** running mirall theme!";
}

QString mirallTheme::appName() const
{
    return QString::fromLocal8Bit("Mirall");
}

QString mirallTheme::configFileName() const
{
    return QString::fromLocal8Bit("mirall.cfg");
}

QPixmap mirallTheme::splashScreen() const
{
    return QPixmap(":/mirall/resources/owncloud_splash.png"); // FIXME: mirall splash!
}

}


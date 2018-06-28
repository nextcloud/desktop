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

#include "nextcloudtheme.h"

#include <QString>
#include <QVariant>
#ifndef TOKEN_AUTH_ONLY
#include <QPixmap>
#include <QIcon>
#endif
#include <QCoreApplication>

#include "config.h"
#include "common/utility.h"
#include "version.h"

namespace OCC {

NextcloudTheme::NextcloudTheme()
    : Theme()
{
}

#ifndef TOKEN_AUTH_ONLY
QVariant NextcloudTheme::customMedia(CustomMediaType)
{
    return QVariant();
}
#endif

#ifndef TOKEN_AUTH_ONLY
QColor NextcloudTheme::wizardHeaderBackgroundColor() const
{
    return QColor("#0082c9");
}

QColor NextcloudTheme::wizardHeaderTitleColor() const
{
    return QColor("#ffffff");
}

QPixmap NextcloudTheme::wizardHeaderLogo() const
{
    return QPixmap(hidpiFileName(":/client/theme/colored/wizard_logo.png"));
}

#endif

}

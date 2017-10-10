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
#include <QStyle>
#include <QApplication>
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

QString NextcloudTheme::configFileName() const
{
    return QLatin1String("nextcloud.cfg");
}

QString NextcloudTheme::about() const
{
     QString re;
     re = tr("<p>Version %1. For more information please visit <a href='%2'>%3</a>.</p>")
              .arg(MIRALL_VERSION_STRING).arg("http://" MIRALL_STRINGIFY(APPLICATION_DOMAIN))
              .arg(MIRALL_STRINGIFY(APPLICATION_DOMAIN));

     re += trUtf8("<p><small>By Klaas Freitag, Daniel Molkentin, Jan-Christoph Borchardt, "
                  "Olivier Goffart, Markus Götz and others.</small></p>");

     re += tr("<p>This release was supplied by the Nextcloud GmbH<br />"
              "Copyright 2012-2017 ownCloud GmbH</p>");

     re += tr("<p>Licensed under the GNU General Public License (GPL) Version 2.0.<br/>"
              "%2 and the %2 Logo are registered trademarks of %1 in the "
              "European Union, other countries, or both.</p>")
              .arg(APPLICATION_VENDOR).arg(APPLICATION_NAME);

    re += gitSHA1();
    return re;
}

#ifndef TOKEN_AUTH_ONLY
QIcon NextcloudTheme::trayFolderIcon(const QString &) const
{
    QPixmap fallback = qApp->style()->standardPixmap(QStyle::SP_FileDialogNewFolder);
    return QIcon::fromTheme("folder", fallback);
}

QIcon NextcloudTheme::applicationIcon() const
{
    return themeIcon(QLatin1String("Nextcloud-icon"));
}


QVariant NextcloudTheme::customMedia(Theme::CustomMediaType type)
{
    if (type == Theme::oCSetupTop) {
        // return QCoreApplication::translate("NextcloudTheme",
        //                                   "If you don't have an ownCloud server yet, "
        //                                   "see <a href=\"https://owncloud.com\">owncloud.com</a> for more info.",
        //                                   "Top text in setup wizard. Keep short!");
        return QVariant();
    } else {
        return QVariant();
    }
}

#endif

QString NextcloudTheme::helpUrl() const
{
    return QString::fromLatin1("https://docs.nextcloud.com/desktop/%1.%2/").arg(MIRALL_VERSION_MAJOR).arg(MIRALL_VERSION_MINOR);
}

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

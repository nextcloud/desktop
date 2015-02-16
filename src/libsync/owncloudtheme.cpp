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
#ifndef TOKEN_AUTH_ONLY
#include <QPixmap>
#include <QIcon>
#include <QStyle>
#include <QApplication>
#endif
#include <QCoreApplication>
#include <QDebug>

#include "version.h"
#include "config.h"

namespace OCC {

ownCloudTheme::ownCloudTheme() :
    Theme()
{
    // qDebug() << " ** running ownCloud theme!";
}

QString ownCloudTheme::configFileName() const
{
    return QLatin1String("owncloud.cfg");
}

QString ownCloudTheme::about() const
{
    QString devString;
    devString = trUtf8("<p>Version %2. For more information visit <a href=\"%3\">%4</a></p>"
               "<p><small>By Klaas Freitag, Daniel Molkentin, Jan-Christoph Borchardt, "
               "Olivier Goffart, Markus Götz and others.</small></p>"
               "<p>Copyright ownCloud, Inc.</p>"
               "<p>Licensed under the GNU General Public License (GPL) Version 2.0<br/>"
               "ownCloud and the ownCloud Logo are registered trademarks of ownCloud, "
               "Inc. in the United States, other countries, or both.</p>"
               )
            .arg(MIRALL_VERSION_STRING)
            .arg("http://" MIRALL_STRINGIFY(APPLICATION_DOMAIN))
            .arg(MIRALL_STRINGIFY(APPLICATION_DOMAIN));

    devString += gitSHA1();
    return devString;

}

#ifndef TOKEN_AUTH_ONLY
QIcon ownCloudTheme::trayFolderIcon( const QString& ) const
{
    QPixmap fallback = qApp->style()->standardPixmap(QStyle::SP_FileDialogNewFolder);
    return QIcon::fromTheme("folder", fallback);
}

QIcon ownCloudTheme::applicationIcon( ) const
{
    return themeIcon( QLatin1String("owncloud-icon") );
}


QVariant ownCloudTheme::customMedia(Theme::CustomMediaType type)
{
    if (type == Theme::oCSetupTop) {
        // return QCoreApplication::translate("ownCloudTheme",
        //                                   "If you don't have an ownCloud server yet, "
        //                                   "see <a href=\"https://owncloud.com\">owncloud.com</a> for more info.",
        //                                   "Top text in setup wizard. Keep short!");
        return QVariant();
    } else {
        return QVariant();
    }
}

#endif

QString ownCloudTheme::helpUrl() const
{
    return QString::fromLatin1("http://doc.owncloud.org/desktop/%1.%2/").arg(MIRALL_VERSION_MAJOR).arg(MIRALL_VERSION_MINOR);
}

#ifndef TOKEN_AUTH_ONLY
QColor ownCloudTheme::wizardHeaderBackgroundColor() const
{
    return QColor("#1d2d42");
}

QColor ownCloudTheme::wizardHeaderTitleColor() const
{
    return QColor("#ffffff");
}

QPixmap ownCloudTheme::wizardHeaderLogo() const
{
    return QPixmap(hidpiFileName(":/client/theme/colored/wizard_logo.png"));
}
#endif

QString ownCloudTheme::appName() const
{
    return QLatin1String("ownCloud");
}

QString ownCloudTheme::appNameGUI() const
{
    return QLatin1String("ownCloud");
}


}


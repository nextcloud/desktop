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

#include "theme.h"
#include "version.h"

#include <QtCore>
#include <QtGui>

namespace Mirall {

Theme::Theme()
{

}

QString Theme::statusHeaderText( SyncResult::Status status ) const
{
    QString resultStr;

    switch( status ) {
    case SyncResult::Undefined:
        resultStr = QObject::tr("Status undefined");
        break;
    case SyncResult::NotYetStarted:
        resultStr = QObject::tr("Waiting to start sync");
        break;
    case SyncResult::SyncRunning:
        resultStr = QObject::tr("Sync is running");
        break;
    case SyncResult::Success:
        resultStr = QObject::tr("Sync Success");
        break;
    case SyncResult::Error:
        resultStr = QObject::tr("Sync Error - Click info button for details.");
        break;
    case SyncResult::SetupError:
        resultStr = QObject::tr( "Setup Error" );
        break;
    default:
        resultStr = QObject::tr("Status undefined");
    }
    return resultStr;
}

QString Theme::version() const
{
    return QString::fromLocal8Bit( MIRALL_STRINGIFY( MIRALL_VERSION ))+QLatin1String("beta1");
}

QIcon Theme::trayFolderIcon( const QString& backend ) const
{
    return folderIcon( backend );
}

/*
 * helper to load a icon from either the icon theme the desktop provides or from
 * the apps Qt resources.
 */
QIcon Theme::themeIcon( const QString& name ) const
{
    QIcon icon;
    if( QIcon::hasThemeIcon( name )) {
        // use from theme
        icon = QIcon::fromTheme( name );
    } else {
        QList<int> sizes;
        sizes <<16 << 24 << 32 << 48 << 64 << 128;
        foreach (int size, sizes) {
            QString pixmapName = QString::fromLatin1(":/mirall/resources/%1-%2.png").arg(name).arg(size);
            if (QFile::exists(pixmapName)) {
                icon.addFile(pixmapName, QSize(size, size));
            }
        }
    }
    return icon;
}

// if this option return true, the client only supports one folder to sync.
// The Add-Button is removed accoringly.
bool Theme::singleSyncFolder() const {
    return false;
}

} // end namespace mirall


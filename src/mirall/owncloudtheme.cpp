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
#include <QDebug>
#include <QPixmap>
#include <QIcon>
#include <QStyle>
#include <QApplication>

namespace Mirall {

ownCloudTheme::ownCloudTheme()
{
    // qDebug() << " ** running ownCloud theme!";
}

QString ownCloudTheme::appName() const
{
    /* If this is changed, existing configs are not found any more
     * because the value is used by QDesktopServices to find the config
     * file. Be aware.
     */
    return QLatin1String("ownCloud");
}

QString ownCloudTheme::configFileName() const
{
    return QLatin1String("owncloud.cfg");
}

QPixmap ownCloudTheme::splashScreen() const
{
    return QPixmap(QLatin1String(":/mirall/resources/owncloud_splash.png"));
}

QIcon ownCloudTheme::folderIcon( const QString& backend ) const
{
  QString name;

  if( backend == QLatin1String("owncloud")) {
      name = QLatin1String( "owncloud-framed" );
  }
  if( backend == QLatin1String("unison" )) {
      name  = QLatin1String( "folder-sync" );
  }
  if( backend == QLatin1String("csync" )) {
      name   = QLatin1String( "folder-remote" );
  }
  if( backend.isEmpty() || backend == QLatin1String("none") ) {
      name = QLatin1String("folder-grey");
  }

  qDebug() << "==> load folder icon " << name;
  return themeIcon( name );
}

QIcon ownCloudTheme::trayFolderIcon( const QString& ) const
{
    QPixmap fallback = qApp->style()->standardPixmap(QStyle::SP_FileDialogNewFolder);
    return QIcon::fromTheme("folder", fallback);
}

QIcon ownCloudTheme::syncStateIcon( SyncResult::Status status, bool sysTray ) const
{
    // FIXME: Mind the size!
    QString statusIcon;

    switch( status ) {
    case SyncResult::Undefined:
        statusIcon = QLatin1String("offline");
        break;
    case SyncResult::NotYetStarted:
        statusIcon = QLatin1String("offline");
        break;
    case SyncResult::SyncRunning:
        statusIcon = QLatin1String("sync");
        break;
    case SyncResult::Success:
        statusIcon = QLatin1String("ok");
        break;
    case SyncResult::Error:
        statusIcon = QLatin1String("error");
        break;
    case SyncResult::SetupError:
        statusIcon = QLatin1String("error");
        break;
    default:
        statusIcon = QLatin1String("error");
    }

    return themeIcon( statusIcon, sysTray );
}

QIcon ownCloudTheme::folderDisabledIcon( ) const
{
    // Fixme: Do we really want the dialog-canel from theme here?
    return themeIcon( QLatin1String("owncloud-error") );
}

QIcon ownCloudTheme::applicationIcon( ) const
{
    return themeIcon( QLatin1String("owncloud-icon") );
}

}


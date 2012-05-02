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
#include <QIcon>

#include "owncloudtheme.h"

namespace Mirall {

ownCloudTheme::ownCloudTheme()
{
    // qDebug() << " ** running ownCloud theme!";
}

QString ownCloudTheme::appName() const
{
    return QString::fromLocal8Bit("ownCloud");
}

QString ownCloudTheme::configFileName() const
{
    return QString::fromLocal8Bit("owncloud.cfg");
}

QPixmap ownCloudTheme::splashScreen() const
{
    return QPixmap(":/mirall/resources/owncloud_splash.png");
}

QIcon ownCloudTheme::folderIcon( const QString& backend ) const
{
  QString name;
  int size = 48;

  if( backend == QString::fromLatin1("owncloud")) {
      name = QString( "owncloud-icon-framed" );
      size = 64;
  }
  if( backend == QString::fromLatin1("unison" )) {
      name  = QString( "folder-sync" );
  }
  if( backend == QString::fromLatin1("csync" )) {
      name   = QString( "folder-remote" );
  }
  if( backend.isEmpty() || backend == QString::fromLatin1("none") ) {
      name = QString("folder-grey.png");
  }

  qDebug() << "==> load folder icon " << name;
  return themeIcon( name, size );
}

QIcon ownCloudTheme::trayFolderIcon( const QString& ) const
{
    return themeIcon( "owncloud-icon", 48 );
}


QIcon ownCloudTheme::syncStateIcon( SyncResult::Status status, int size ) const
{
    // FIXME: Mind the size!
    QString statusIcon;

    switch( status ) {
    case SyncResult::Undefined:
        statusIcon = "owncloud-icon-error";
        break;
    case SyncResult::NotYetStarted:
        statusIcon = "owncloud-icon";
        break;
    case SyncResult::SyncRunning:
        statusIcon = "owncloud-icon-sync";
        break;
    case SyncResult::Success:
        statusIcon = "owncloud-sync-ok";
        break;
    case SyncResult::Error:
        statusIcon = "owncloud-icon-error";
        break;
    case SyncResult::SetupError:
        statusIcon = "owncloud-icon-error";
        break;
    default:
        statusIcon = "owncloud-icon-error";
    }

    return themeIcon( statusIcon, size );
}


QIcon ownCloudTheme::folderDisabledIcon( int size ) const
{
    // Fixme: Do we really want the dialog-canel from theme here?
    return themeIcon( "owncloud-icon-error", size );
}

QIcon ownCloudTheme::applicationIcon( ) const
{
    return themeIcon( "owncloud-icon", 48 );
}


}


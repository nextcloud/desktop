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

#include "miralltheme.h"

#include <QString>
#include <QDebug>
#include <QPixmap>
#include <QIcon>

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

QIcon mirallTheme::folderIcon( const QString& backend ) const
{
  QString name;
  int size = 48;

  if( backend == QString::fromLatin1("owncloud")) {
      name = QString( "mirall" );
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

QIcon mirallTheme::syncStateIcon( SyncResult::Status status, int size ) const
{
    QString statusIcon;

    switch( status ) {
    case SyncResult::Undefined:
        statusIcon = "dialog-close";
        break;
    case SyncResult::NotYetStarted:
        statusIcon = "task-ongoing";
        break;
    case SyncResult::SyncRunning:
        statusIcon = "view-refresh";
        break;
    case SyncResult::Success:
        statusIcon = "dialog-ok";
        break;
    case SyncResult::Error:
        statusIcon = "dialog-close";
        break;
    case SyncResult::SetupError:
        statusIcon = "dialog-cancel";
        break;
    default:
        statusIcon = "dialog-close";
    }
    return themeIcon( statusIcon, size );
}


QIcon mirallTheme::folderDisabledIcon( int size ) const
{
    // Fixme: Do we really want the dialog-canel from theme here?
    return themeIcon( "dialog-cancel", size );
}

QIcon mirallTheme::applicationIcon( ) const
{
    return themeIcon( "mirall", 48 );
}

}


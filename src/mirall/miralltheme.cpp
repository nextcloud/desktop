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
    return QLatin1String("Mirall");
}

QString mirallTheme::configFileName() const
{
    return QLatin1String("mirall.cfg");
}

QPixmap mirallTheme::splashScreen() const
{
    return QPixmap(QLatin1String(":/mirall/resources/owncloud_splash.png")); // FIXME: mirall splash!
}

QIcon mirallTheme::folderIcon( const QString& backend ) const
{
  QString name;

  if( backend == QString::fromLatin1("owncloud")) {
      name = QLatin1String( "mirall" );
  }
  if( backend == QString::fromLatin1("unison" )) {
      name  = QLatin1String( "folder-sync" );
  }
  if( backend == QString::fromLatin1("csync" )) {
      name   = QLatin1String( "folder-remote" );
  }
  if( backend.isEmpty() || backend == QString::fromLatin1("none") ) {
      name = QLatin1String("folder-grey.png");
  }

  qDebug() << "==> load folder icon " << name;
  return themeIcon( name );
}

QIcon mirallTheme::syncStateIcon( SyncResult::Status status, bool sysTray ) const
{
    QString statusIcon;

    switch( status ) {
    case SyncResult::Undefined:
        statusIcon = QLatin1String("dialog-close");
        break;
    case SyncResult::NotYetStarted:
    case SyncResult::SyncPrepare:
        statusIcon = QLatin1String("task-ongoing");
        break;
    case SyncResult::SyncRunning:
        statusIcon = QLatin1String("view-refresh");
        break;
    case SyncResult::Success:
        statusIcon = QLatin1String("dialog-ok");
        break;
    case SyncResult::Error:
        statusIcon = QLatin1String("dialog-close");
        break;
    case SyncResult::SetupError:
        statusIcon = QLatin1String("dialog-cancel");
        break;
    default:
        statusIcon = QLatin1String("dialog-close");
    }
    return themeIcon( statusIcon, sysTray );
}


QIcon mirallTheme::folderDisabledIcon() const
{
    // Fixme: Do we really want the dialog-canel from theme here?
    return themeIcon( QLatin1String("dialog-cancel") );
}

QIcon mirallTheme::applicationIcon( ) const
{
    return themeIcon( QLatin1String("mirall"));
}

}


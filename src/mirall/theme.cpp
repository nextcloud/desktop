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
    :QObject()
{

}

QIcon Theme::folderIcon( const QString& backend, int size ) const
{
  QString name;

  if( backend == QString::fromLatin1("owncloud")) name = QString( "mirall-%1.png" ).arg(size);
  if( backend == QString::fromLatin1("unison" )) name  = QString( "folder-%1.png" ).arg(size);
  if( backend == QString::fromLatin1("csync" )) name   = QString( "folder-remote-%1.png" ).arg(size);
  if( backend.isEmpty() || backend == QString::fromLatin1("none") ) {
      // name = QString("folder-grey-%1.png").arg(size);
      name = QString("folder-grey-48.png");
  }

  qDebug() << "==> load folder icon " << name;
  return QIcon( QPixmap( QString(":/mirall/resources/%1").arg(name)) );
}

QIcon Theme::syncStateIcon( SyncResult::Status status, int ) const
{
    // FIXME: Mind the size!
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
    return QIcon::fromTheme( statusIcon, QIcon( QString( ":/mirall/resources/%1").arg(statusIcon) ) );
}

QIcon Theme::folderDisabledIcon() const
{
    // Fixme: Do we really want the dialog-canel from theme here?
    return QIcon::fromTheme( "dialog-cancel", QIcon( QString( ":/mirall/resources/dialog-cancel")) );
}

QString Theme::statusHeaderText( SyncResult::Status status ) const
{
    QString resultStr;

    switch( status ) {
    case SyncResult::Undefined:
        resultStr = tr("Status undefined");
        break;
    case SyncResult::NotYetStarted:
        resultStr = tr("Waiting to start sync");
        break;
    case SyncResult::SyncRunning:
        resultStr = tr("Sync is running");
        break;
    case SyncResult::Success:
        resultStr = tr("Sync Success");
        break;
    case SyncResult::Error:
        resultStr = tr("Sync Error - Click info button for details.");
        break;
    case SyncResult::SetupError:
        resultStr = tr( "Setup Error" );
        break;
    default:
        resultStr = tr("Status undefined");
    }
    return resultStr;
}

QIcon Theme::applicationIcon( ) const
{
    return QIcon(QString(":mirall/resources/mirall-48"));
}

QString Theme::version() const
{
    return QString::fromLocal8Bit( MIRALL_STRINGIFY( MIRALL_VERSION ) );
}

}


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

  if( backend == "owncloud") name = QString( "mirall-%1.png" ).arg(size);
  if( backend == "unison" ) name  = QString( "folder-%1.png" ).arg(size);
  if( backend == "csync" ) name   = QString( "folder-remote-%1.png" ).arg(size);

  return QIcon( QString( ":/mirall/resources/%1").arg(name) );
}

QIcon Theme::syncStateIcon( SyncResult::Status status, int ) const
{
    // FIXME: Mind the size!
    QString statusIcon;

    qDebug() << "Status: " << status;

    if( status == SyncResult::NotYetStarted ) {
        statusIcon = "dialog-close";
    } else if( status == SyncResult::SyncRunning ) {
        statusIcon = "view-refresh";
    } else if( status == SyncResult::Success ) {
        statusIcon = "dialog-ok";
    } else if( status == SyncResult::Error ) {
        statusIcon = "dialog-close";
    } else if( status == SyncResult::Disabled ) {
        statusIcon = "dialog-cancel";
    } else if( status == SyncResult::SetupError ) {
        statusIcon = "dialog-cancel";
    } else {
        statusIcon = "dialog-close";
    }
    return QIcon::fromTheme( statusIcon, QIcon( QString( ":/mirall/resources/%1").arg(statusIcon) ) );
}

QString Theme::statusHeaderText( SyncResult::Status status ) const
{
    QString resultStr;

    if( status == SyncResult::NotYetStarted ) {
        resultStr = tr("Not yet started");
    } else if( status == SyncResult::SyncRunning ) {
        resultStr = tr("Sync running");
    } else if( status == SyncResult::Success ) {
        resultStr = tr("Success");
    } else if( status == SyncResult::Error ) {
        resultStr = tr("Error");
    } else if( status == SyncResult::Disabled ) {
        resultStr = tr("Disabled");
    } else if( status == SyncResult::SetupError ) {
        resultStr = tr( "Setup Error" );
    } else {
        resultStr = tr("Undefined");
    }
    return resultStr;
}

QIcon Theme::applicationIcon( int size ) const
{
    return QIcon::fromTheme( "mirall", QIcon(QString(":mirall/resources/mirall-%1").arg(size) ) );
}

QString Theme::version() const
{
    return QString::fromLocal8Bit( MIRALL_STRINGIFY( MIRALL_VERSION ) );
}

}


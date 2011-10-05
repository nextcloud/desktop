/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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

#include "owncloudinfo.h"
#include "QtCore"
#include "QtGui"

namespace Mirall
{

ownCloudInfo::ownCloudInfo(QObject *parent) :
    QObject(parent)
{

}

QString ownCloudInfo::configFile() const
{
  return QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/mirall.cfg";
}

bool ownCloudInfo::isConfigured()
{
  QSettings settings( configFile(), QSettings::IniFormat );

  if( settings.contains("ownCloud/url") &&
      settings.contains("ownCloud/user") &&
      settings.contains("ownCloud/password") ) {
    return true;
  }
  return false;
}

QString ownCloudInfo::url() const
{
  QSettings settings( configFile(), QSettings::IniFormat );
  return settings.value("ownCloud/url" ).toString();
}

QString ownCloudInfo::user() const
{
  QSettings settings( configFile(), QSettings::IniFormat );
  return settings.value( "ownCloud/user" ).toString();
}

QString ownCloudInfo::password() const
{
  QSettings settings( configFile(), QSettings::IniFormat );
  return settings.value( "ownCloud/password" ).toString();
}

void ownCloudInfo::checkInstallation()
{
 QNetworkAccessManager *manager = new QNetworkAccessManager(this);
 connect(manager, SIGNAL(finished(QNetworkReply*)),
         this, SLOT(slotReplyFinished(QNetworkReply*)));

 _reply = manager->get(QNetworkRequest(QUrl( url() + "/status.php")));
 connect( _reply, SIGNAL( error(QNetworkReply::NetworkError )),
          this, SLOT(slotError( QNetworkReply::NetworkError )));
 connect( _reply, SIGNAL( readyRead()), this, SLOT(slotReadyRead()));
}



void ownCloudInfo::slotReplyFinished( QNetworkReply *reply )
{
  const QString version( _readBuffer );
  const QString url = reply->url().toString();

  emit ownCloudInfoFound( url, version );
}

void ownCloudInfo::slotReadyRead()
{
  _readBuffer.append(_reply->readAll());
}

void ownCloudInfo::slotError( QNetworkReply::NetworkError )
{
  emit noOwncloudFound();
}

}

#include "owncloudinfo.moc"

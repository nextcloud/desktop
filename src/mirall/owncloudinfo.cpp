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

#include <QtCore>
#include <QtGui>
#include <QAuthenticator>


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
  QString url = settings.value("ownCloud/url").toString();
  if( url.endsWith( QChar('/')) ) url.remove( -1, 1);
  return url;
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
 _readBuffer.clear();

 connect( _reply, SIGNAL( error(QNetworkReply::NetworkError )),
          this, SLOT(slotError( QNetworkReply::NetworkError )));
 connect( _reply, SIGNAL( readyRead()), this, SLOT(slotReadyRead()));
}

void ownCloudInfo::slotAuthentication( QNetworkReply*, QAuthenticator *auth )
{
  if( auth ) {
    qDebug() << "Authenticating request!";
    auth->setUser( user() );
    auth->setPassword( password() );
  }
}

void ownCloudInfo::slotReplyFinished( QNetworkReply *reply )
{
  const QString version( _readBuffer );
  const QString url = reply->url().toString();

  QString info( version );

  if( info.contains("installed") && info.contains("version") && info.contains("versionstring") ) {
    info.remove(0,1); // remove first char which is a "{"
    info.remove(-1,1); // remove the last char which is a "}"
    QStringList li = info.split( QChar(',') );

    QString infoString;
    QString versionStr;
    foreach ( infoString, li ) {
      if( infoString.contains( "versionstring") ) {
        // get the version string out.
        versionStr = infoString.mid(17);
        versionStr.remove(-1, 1);
      }
    }
    QString urlStr( url );
    urlStr.remove("/status.php"); // get the plain url.

    emit ownCloudInfoFound( urlStr, versionStr );
  } else {
    qDebug() << "No proper answer on status.php!";
    emit noOwncloudFound();
  }
}

void ownCloudInfo::slotReadyRead()
{
  _readBuffer.append(_reply->readAll());
}

void ownCloudInfo::slotError( QNetworkReply::NetworkError err)
{
  qDebug() << "Network Error: " << err;
  emit noOwncloudFound();
}

}

#include "owncloudinfo.moc"

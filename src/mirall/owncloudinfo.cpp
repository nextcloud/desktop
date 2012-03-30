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


#include <QtCore>
#include <QtGui>
#include <QAuthenticator>

#include "mirall/owncloudinfo.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/version.h"

namespace Mirall
{

ownCloudInfo::ownCloudInfo( const QString& connectionName, QObject *parent ) :
    QObject(parent),
    _reply(0)
{
    if( connectionName.isEmpty() )
        _connection = QString::fromLocal8Bit( "ownCloud");
    else
        _connection = connectionName;

    _manager = new QNetworkAccessManager;
    connect(_manager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(slotReplyFinished(QNetworkReply*)));
}

ownCloudInfo::~ownCloudInfo()
{
    delete _manager;
}

bool ownCloudInfo::isConfigured()
{
    MirallConfigFile cfgFile;
    return cfgFile.connectionExists( _connection );
}

void ownCloudInfo::checkInstallation()
{
    getRequest( "status.php", false );
}

void ownCloudInfo::getWebDAVPath( const QString& path )
{
    getRequest( path, true );
}

void ownCloudInfo::getRequest( const QString& path, bool webdav )
{
    qDebug() << "Get Request to " << path;
    if( _reply ) {
        if(_reply->isRunning() ) {
            qDebug() << "info request already running, not starting a second one.";
            return;
        } else {
            delete _reply;
        }
    }
    // this is not a status call.
    if( !webdav && path == "status.php") {
        _versionInfoCall = true;
        _directory.clear();
    } else {
        _directory = path;
        _versionInfoCall = false;
    }

    MirallConfigFile cfgFile;
    QString url = cfgFile.ownCloudUrl( _connection, webdav ) + path;
    QNetworkRequest request;
    request.setUrl( QUrl( url ) );
    request.setRawHeader( "User-Agent", QString("mirall-%1").arg(MIRALL_STRINGIFY(MIRALL_VERSION)).toAscii());
    request.setRawHeader( "Authorization", cfgFile.basicAuthHeader() );
    _readBuffer.clear();
    _reply = _manager->get( request );

    connect( _reply, SIGNAL( error(QNetworkReply::NetworkError )),
             this, SLOT(slotError( QNetworkReply::NetworkError )));
    connect( _reply, SIGNAL( readyRead()), this, SLOT(slotReadyRead()));
}


void ownCloudInfo::slotAuthentication( QNetworkReply*, QAuthenticator *auth )
{
    if( auth ) {
        MirallConfigFile cfgFile;
        qDebug() << "Authenticating request!";
        auth->setUser( cfgFile.ownCloudUser( _connection ) );
        auth->setPassword( cfgFile.ownCloudPasswd( _connection ));
    }
}

void ownCloudInfo::slotReplyFinished( QNetworkReply *reply )
{
  const QString version( _readBuffer );
  const QString url = reply->url().toString();

  QString info( version );

  if( _versionInfoCall ) {
      // it was a call to status.php

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
          emit noOwncloudFound( reply->error() );
      }
  } else {
      // it was a general GET request.
      emit ownCloudDirExists( _directory, reply );
  }
}

void ownCloudInfo::slotReadyRead()
{
    // do not slurp in the content of other calls.
    if( _versionInfoCall ) {
        _readBuffer.append(_reply->readAll());
    }
}

void ownCloudInfo::slotError( QNetworkReply::NetworkError err)
{
  qDebug() << "ownCloudInfo Network Error: " << err;
  // emit noOwncloudFound( err );
}

}


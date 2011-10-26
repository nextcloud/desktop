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

#include "mirallwebdav.h"

#include <QtCore>
#include <QtNetwork>
#include <QObject>

MirallWebDAV::MirallWebDAV(QObject *parent) :
    QObject(parent)
{
  _webdav = new QWebdav;
  connect( _webdav, SIGNAL(webdavFinished(QNetworkReply*)), this,
           SIGNAL(webdavFinished(QNetworkReply*)) );

}

bool MirallWebDAV::httpConnect( const QString& str, const QString& user, const QString& passwd)
{
  _host = str;
  if( !_host.endsWith( "webdav.php")) {
    _host.append( "/files/webdav.php");
  }
  _webdav->init( _host, user, passwd );
}

bool MirallWebDAV::mkdir( const QString& dir )
{
  bool re = true;

  QNetworkReply *reply = _webdav->mkdir( dir );
  if( reply->error() != QNetworkReply::NoError ) {
    qDebug() << "WebDAV Mkdir failed.";
    re = false;
  }
  return re;
}

#include "mirallwebdav.moc"

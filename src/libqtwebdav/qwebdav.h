/* This file is part of QWebdav
 *
 * Copyright (C) 2009-2010 Corentin Chary <corentin.chary@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef QWEBDAV_H
#define QWEBDAV_H

#include <QNetworkAccessManager>
#include <QUrlInfo>
#include <QDateTime>
#include <QDomNodeList>
#include <QMap>
#include <QNetworkReply>

#include "qwebdav_export.h"

class QWebdavUrlInfo;
class QNetworkReply;

/**
 * @brief Main class used to handle the webdav protocol
 */
class QWEBDAV_EXPORT QWebdav : virtual public QNetworkAccessManager
{
  Q_OBJECT
 public:
  QWebdav (QObject * parent = 0 );
  ~QWebdav ();

  void init(const QString & hostName, const QString & user, const QString & passwd);



  typedef QMap < QString, QMap < QString, QVariant > > PropValues;
  typedef QMap < QString , QStringList > PropNames;

  QNetworkReply* list ( const QString & dir = QString() );
  QNetworkReply* search ( const QString & path, const QString & query );
  QNetworkReply* put ( const QString & path, QIODevice *data );
  QNetworkReply* put ( const QString & path, QByteArray & data );

  QNetworkReply* mkcol ( const QString & dir );

  QNetworkReply* mkdir ( const QString & dir );
  QNetworkReply* copy ( const QString & oldname, const QString & newname,
	     bool overwrite = false );
  QNetworkReply* rename ( const QString & oldname, const QString & newname,
	       bool overwrite = false );
  QNetworkReply* move ( const QString & oldname, const QString & newname,
	     bool overwrite = false );
  QNetworkReply* rmdir ( const QString & dir );
  QNetworkReply* remove ( const QString & path );

  QNetworkReply* propfind ( const QString & path, const QByteArray & query, int depth = 0 );
  QNetworkReply* propfind ( const QString & path, const QWebdav::PropNames & props,
		 int depth = 0 );

  QNetworkReply* proppatch ( const QString & path, const QWebdav::PropValues & props);
  QNetworkReply* proppatch ( const QString & path, const QByteArray & query );

  int setHost ( const QString &, quint16 );

  /* TODO lock, unlock */
 signals:
  void listInfo ( const QWebdavUrlInfo & i );
  void webdavFinished( QNetworkReply *reply );

 private slots:
  void slotFinished ( QNetworkReply * resp );
  void slotAuthenticate( QNetworkReply*, QAuthenticator* );

 private:

  void emitListInfos();
  void davParsePropstats( const QDomNodeList & propstat );
  int codeFromResponse( const QString& response );
  QDateTime parseDateTime( const QString& input, const QString& type );
  QNetworkReply* davRequest(const QString & reqVerb,
                 QNetworkRequest & req,
                 const QByteArray & data = QByteArray());
  QNetworkReply* davRequest(const QString & reqVerb,
                 QNetworkRequest & req,
		 QIODevice * data);
  void setupHeaders(QNetworkRequest & req, quint64 size);

 private:
  Q_DISABLE_COPY(QWebdav);
  bool emitListInfo;
  QByteArray buffer;
  QString _host;
  QString _user;
  QString _passwd;

  QString _lastError;
  QNetworkReply::NetworkError _lastReplyCode;

  int     _authenticateCounter;
};

#endif // QWEBDAV_H

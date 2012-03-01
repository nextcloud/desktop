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

#include <QDomDocument>
#include <QDomNode>
#include <QUrl>
#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QBuffer>
#include <QStringList>
#include <QAuthenticator>

#include "qwebdav.h"
#include "qwebdav_url_info.h"

QWebdav::QWebdav (QObject *parent)
  : QNetworkAccessManager(parent),
    _authenticateCounter(0)
{

}

QWebdav::~QWebdav ()
{
}

void QWebdav::init(const QString & hostName, const QString& user, const QString& passwd )
{
  _host = hostName;
  _user = user;
  _passwd = passwd;

  emitListInfo = false;
  connect(this, SIGNAL(finished ( QNetworkReply* )),
          this, SLOT(slotFinished ( QNetworkReply* )));

  connect(this, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
          SLOT(slotAuthenticate(QNetworkReply*, QAuthenticator*)));
}

void QWebdav::slotAuthenticate( QNetworkReply *reply, QAuthenticator *auth )
{
  qDebug() << "!!!! Authentication required, setting " << _user;

  _authenticateCounter++;
  if( _authenticateCounter > 10 ) {
    // we are in a loop of authentication counters.
    reply->abort();
    slotFinished( reply );
  }
  if( auth ) {
    auth->setUser( _user );
    auth->setPassword( _passwd );
  }

}

QNetworkReply* QWebdav::list ( const QString & dir)
{
  QWebdav::PropNames query;
  QStringList props;

  props << "creationdate";
  props << "getcontentlength";
  props << "displayname";
  props << "source";
  props << "getcontentlanguage";
  props << "getcontenttype";
  props << "executable";
  props << "getlastmodified";
  props << "getetag";
  props << "resourcetype";

  query["DAV:"] = props;

  return propfind(dir, query, 1);
}

QNetworkReply*
QWebdav::search ( const QString & path, const QString & q )
{
  QByteArray query = "<?xml version=\"1.0\"?>\r\n";

  query.append( "<D:searchrequest xmlns:D=\"DAV:\">\r\n" );
  query.append( q.toUtf8() );
  query.append( "</D:searchrequest>\r\n" );

  QNetworkRequest req;
  req.setUrl(path);

  return davRequest("SEARCH", req);
}

QNetworkReply*
QWebdav::put ( const QString & path, QIODevice * data )
{
  QNetworkRequest req;
  req.setUrl(QUrl(path));

  return davRequest("PUT", req, data);
}

QNetworkReply*
QWebdav::put ( const QString & path, QByteArray & data )
{
  QBuffer buffer(&data);

  return put(path, &buffer);
}

QNetworkReply*
QWebdav::propfind ( const QString & path, const QWebdav::PropNames & props,
		    int depth)
{
  QByteArray query;

  query = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>";
  query += "<D:propfind xmlns:D=\"DAV:\" >";
  query += "<D:prop>";
  foreach (QString ns, props.keys())
    {
      foreach (const QString key, props[ns])
	if (ns == "DAV:")
	  query += "<D:" + key + "/>";
	else
      	  query += "<" + key + " xmlns=\"" + ns + "\"/>";
    }
  query += "</D:prop>";
  query += "</D:propfind>";
  return propfind(path, query, depth);
}


QNetworkReply*
QWebdav::propfind( const QString & path, const QByteArray & query, int depth )
{
  QNetworkRequest req;

  req.setUrl(QUrl(path));

  QString value;

  if (depth == 2)
    value = "infinity";
  else
    value = QString("%1").arg(depth);
  req.setRawHeader(QByteArray("Depth"), value.toUtf8());
  return davRequest("PROPFIND", req, query);
}

QNetworkReply*
QWebdav::proppatch ( const QString & path, const QWebdav::PropValues & props)
{
  QByteArray query;

  query = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>";
  query += "<D:proppatch xmlns:D=\"DAV:\" >";
  query += "<D:prop>";
  foreach (QString ns, props.keys())
    {
      QMap < QString , QVariant >::const_iterator i;

      for (i = props[ns].constBegin(); i != props[ns].constEnd(); ++i) {
	if (ns == "DAV:") {
	  query += "<D:" + i.key() + ">";
	  query += i.value().toString();
	  query += "</D:" + i.key() + ">" ;
	} else {
	  query += "<" + i.key() + " xmlns=\"" + ns + "\">";
	  query += i.value().toString();
	  query += "</" + i.key() + " xmlns=\"" + ns + "\"/>";
	}
      }
    }
  query += "</D:prop>";
  query += "</D:propfind>";

  return proppatch(path, query);
}

QNetworkReply*
QWebdav::proppatch( const QString & path, const QByteArray & query)
{
  QNetworkRequest req;
  req.setUrl(QUrl(path));

  return davRequest("PROPPATCH", req, query);
}

void
QWebdav::emitListInfos()
{
  QDomDocument multiResponse;
  bool hasResponse = false;

  multiResponse.setContent(buffer, true);

  for ( QDomNode n = multiResponse.documentElement().firstChild();
        !n.isNull(); n = n.nextSibling())
    {
      QDomElement thisResponse = n.toElement();

      if (thisResponse.isNull())
	continue;

      QWebdavUrlInfo info(thisResponse);

      if (!info.isValid())
	continue;

      hasResponse = true;
      emit listInfo(info);
    }
}

void
QWebdav::slotFinished ( QNetworkReply * reply )
{
  qDebug() << "WebDAV finished: " << reply->error();
  _lastError = reply->errorString();
  _lastReplyCode = reply->error();

  if( reply->error() != QNetworkReply::NoError ) {
    qDebug() << "Network error: " << reply->errorString();
  }
  if (emitListInfo && reply->error() == QNetworkReply::NoError)
    emitListInfos();
  buffer.clear();
  emitListInfo = false;

  emit webdavFinished( reply );
}


void
QWebdav::setupHeaders(QNetworkRequest & req, quint64 size)
{
  QUrl url( _host );
  qDebug() << "Setting up host header: " << url.host().toUtf8();
  req.setRawHeader(QByteArray("Host"), url.host().toUtf8());
  req.setRawHeader(QByteArray("Connection"), QByteArray("Keep-Alive"));
  if (size) {
      req.setHeader(QNetworkRequest::ContentLengthHeader, QVariant(size));
      req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("text/xml; charset=utf-8"));
  }
}

QNetworkReply*
QWebdav::davRequest(const QString & reqVerb,  QNetworkRequest & req, const QByteArray & data)
{
  QByteArray dataClone(data);
  QBuffer buffer(&dataClone);
  return davRequest(reqVerb, req, &buffer);
}

QNetworkReply*
QWebdav::davRequest(const QString & reqVerb,  QNetworkRequest & req, QIODevice * data)
{
    setupHeaders(req, data ? data->size() : 0);
    return sendCustomRequest(req, reqVerb.toUtf8(), data);
}

QNetworkReply*
QWebdav::mkdir ( const QString & dir )
{
  QNetworkRequest req;
  qDebug() << "Making dir " << dir;

  req.setUrl( QUrl( dir ) );
  return davRequest("MKCOL", req, 0);
}

QNetworkReply*
QWebdav::copy ( const QString & oldname, const QString & newname, bool overwrite)
{
  QNetworkRequest req;

  req.setUrl(QUrl(oldname));
  req.setRawHeader(QByteArray("Destination"), newname.toUtf8());
  req.setRawHeader(QByteArray("Depth"), QByteArray("infinity"));
  req.setRawHeader(QByteArray("Overwrite"), QByteArray(overwrite ? "T" : "F"));
  return davRequest("COPY", req);
}

QNetworkReply*
QWebdav::rename ( const QString & oldname, const QString & newname, bool overwrite)
{
  return move(oldname, newname, overwrite);
}

QNetworkReply*
QWebdav::move ( const QString & oldname, const QString & newname, bool overwrite)
{
  QNetworkRequest req;
  req.setUrl(QUrl(oldname));

  req.setRawHeader(QByteArray("Destination"), newname.toUtf8());
  req.setRawHeader(QByteArray("Depth"), QByteArray("infinity"));
  req.setRawHeader(QByteArray("Overwrite"), QByteArray(overwrite ? "T" : "F"));
  return davRequest("MOVE", req);
}

QNetworkReply*
QWebdav::rmdir ( const QString & dir )
{
  return remove(dir);
}

QNetworkReply*
QWebdav::remove ( const QString & path )
{
  QNetworkRequest req;
  req.setUrl(QUrl(path));
  return davRequest("DELETE", req);
}


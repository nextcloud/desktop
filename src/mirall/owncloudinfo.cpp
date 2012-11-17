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

#include "mirall/owncloudinfo.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/version.h"
#include "mirall/theme.h"
#include "mirall/credentialstore.h"

#include <QtCore>
#include <QtGui>
#include <QAuthenticator>

#if QT46_IMPL
#include <QHttp>
#endif

namespace Mirall
{

ownCloudInfo *ownCloudInfo::_instance = 0;

ownCloudInfo* ownCloudInfo::instance()
{
  static QMutex mutex;
  if (!_instance)
  {
    mutex.lock();

    if (!_instance) {
      _instance = new ownCloudInfo;
    }
    mutex.unlock();
  }

  return _instance;
}

ownCloudInfo::ownCloudInfo() :
    QObject(0)
{
    _connection = Theme::instance()->appName();

    _manager = new QNetworkAccessManager( this );

    MirallConfigFile cfg( _configHandle );
    QSettings settings( cfg.configFile(), QSettings::IniFormat);
    QByteArray certs = settings.value(QLatin1String("CaCertificates")).toByteArray();
    QSslSocket::addDefaultCaCertificates(QSslCertificate::fromData(certs));

    connect( _manager, SIGNAL( sslErrors(QNetworkReply*, QList<QSslError>)),
             this, SIGNAL(sslFailed(QNetworkReply*, QList<QSslError>)) );

    connect( _manager, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)),
             this, SLOT(slotAuthentication(QNetworkReply*,QAuthenticator*)));

    _certsUntrusted = false;
}

ownCloudInfo::~ownCloudInfo()
{
}

void ownCloudInfo::setCustomConfigHandle( const QString& handle )
{
    _configHandle = handle;
    _authAttempts = 0; // allow a couple of tries again.
    resetSSLUntrust();
}

bool ownCloudInfo::isConfigured()
{
    MirallConfigFile cfgFile( _configHandle );
    return cfgFile.connectionExists( _connection );
}

void ownCloudInfo::checkInstallation()
{
    /* No authentication required for this. */
    getRequest( QLatin1String("status.php"), false );
}

void ownCloudInfo::getWebDAVPath( const QString& path )
{
    getRequest( path, true );
}

void ownCloudInfo::getRequest( const QString& path, bool webdav )
{
    qDebug() << "Get Request to " << path;

    MirallConfigFile cfgFile(  _configHandle );
    QString url = cfgFile.ownCloudUrl( _connection, webdav ) + path;
    QNetworkRequest request;
    request.setUrl( QUrl( url ) );
    setupHeaders( request, 0 );

    QNetworkReply *reply = _manager->get( request );
    connect( reply, SIGNAL(finished()), SLOT(slotReplyFinished()));
    _directories[reply] = path;

    if( !_configHandle.isEmpty() ) {
        qDebug() << "Setting config handle " << _configHandle;
        _configHandleMap[reply] = _configHandle;
    }

    connect( reply, SIGNAL( error(QNetworkReply::NetworkError )),
             this, SLOT(slotError( QNetworkReply::NetworkError )));
}

#if QT46_IMPL
void ownCloudInfo::mkdirRequest( const QString& dir )
{
    qDebug() << "OCInfo Making dir " << dir;

    MirallConfigFile cfgFile( _configHandle );
    QUrl url = QUrl( cfgFile.ownCloudUrl( _connection, true ) + dir );
    QHttp::ConnectionMode conMode = QHttp::ConnectionModeHttp;
    if (url.scheme() == "https")
        conMode = QHttp::ConnectionModeHttps;

    QHttp* qhttp = new QHttp(QString(url.encodedHost()), conMode, 0, this);
    qhttp->setUser( CredentialStore::instance()->user(_connection),
                    CredentialStore::instance()->password(_connection) );

    connect(qhttp, SIGNAL(requestStarted(int)), this,SLOT(qhttpRequestStarted(int)));
    connect(qhttp, SIGNAL(requestFinished(int, bool)), this,SLOT(qhttpRequestFinished(int,bool)));
    connect(qhttp, SIGNAL(responseHeaderReceived(QHttpResponseHeader)), this, SLOT(qhttpResponseHeaderReceived(QHttpResponseHeader)));
    //connect(qhttp, SIGNAL(authenticationRequired(QString,quint16,QAuthenticator*)), this, SLOT(qhttpAuthenticationRequired(QString,quint16,QAuthenticator*)));

    QHttpRequestHeader header("MKCOL", QString(url.encodedPath()), 1,1);   /* header */
    header.setValue("Host", QString(url.encodedHost());
    header.setValue("User-Agent", QString("mirall-%1").arg(MIRALL_STRINGIFY(MIRALL_VERSION)).toAscii() );
    header.setValue("Accept-Charset", "ISO-8859-1,utf-8;q=0.7,*;q=0.7");
    header.setValue("Accept-Language", "it,de-de;q=0.8,it-it;q=0.6,en-us;q=0.4,en;q=0.2");
    header.setValue("Connection", "keep-alive");
    header.setContentType("application/x-www-form-urlencoded"); //important
    header.setContentLength(0);
    header.setValue("Authorization", CredentialStore::instance()->basicAuthHeader());

    int david = qhttp->request(header,0,0);
    //////////////// connect(davinfo, SIGNAL(dataSendProgress(int,int)), this, SLOT(SendStatus(int, int)));
    /////////////////connect(davinfo, SIGNAL(done(bool)), this,SLOT(DavWake(bool)));
    //connect(_http, SIGNAL(requestFinished(int, bool)), this,SLOT(qhttpRequestFinished(int,bool)));
    ///////////connect(davinfo, SIGNAL(responseHeaderReceived(constQHttpResponseHeader &)), this, SLOT(RegisterBackHeader(constQHttpResponseHeader &)));


}

void ownCloudInfo::qhttpResponseHeaderReceived(const QHttpResponseHeader& header)
{
    qDebug() << "Resp:" << header.toString();
    if (header.statusCode() == 201)
        emit webdavColCreated( QNetworkReply::NoError );
    else
        qDebug() << "http request failed" << header.toString();
}

void ownCloudInfo::qhttpRequestStarted(int id)
{
    qDebug() << "QHttp based request started " << id;
}

void ownCloudInfo::qhttpRequestFinished(int id, bool success )
{
     qDebug() << "HIT!";
     QHttp* qhttp = qobject_cast<QHttp*>(sender());

     if( success ) {
         qDebug() << "QHttp based request successful";
     } else {
         qDebug() << "QHttp based request failed: " << qhttp->errorString();
     }
}
#else
void ownCloudInfo::mkdirRequest( const QString& dir )
{
    qDebug() << "OCInfo Making dir " << dir;
    _authAttempts = 0;
    MirallConfigFile cfgFile( _configHandle );
    QNetworkRequest req;
    req.setUrl( QUrl( cfgFile.ownCloudUrl( _connection, true ) + dir ) );
    QNetworkReply *reply = davRequest(QLatin1String("MKCOL"), req, 0);

    // remember the confighandle used for this request
    if( ! _configHandle.isEmpty() )
        qDebug() << "Setting config handle " << _configHandle;
        _configHandleMap[reply] = _configHandle;

    if( reply->error() != QNetworkReply::NoError ) {
        qDebug() << "mkdir request network error: " << reply->errorString();
    }

    connect( reply, SIGNAL(finished()), SLOT(slotMkdirFinished()) );
    connect( reply, SIGNAL( error(QNetworkReply::NetworkError )),
             this, SLOT(slotError(QNetworkReply::NetworkError )));
}

void ownCloudInfo::slotMkdirFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    if( ! reply ) {
        qDebug() << "ownCloudInfo: Reply empty!";
        return;
    }

    emit webdavColCreated( reply->error() );
    qDebug() << "mkdir slot hit with status: " << reply->error();
    if( _configHandleMap.contains( reply ) ) {
        _configHandleMap.remove( reply );
    }

    reply->deleteLater();
}
#endif

void ownCloudInfo::slotAuthentication( QNetworkReply *reply, QAuthenticator *auth )
{
    if( !(auth && reply) ) return;
    QString configHandle;

    // an empty config handle is ok for the default config.
    if( _configHandleMap.contains(reply) ) {
        configHandle = _configHandleMap[reply];
        qDebug() << "Auth: Have a custom config handle: " << configHandle;
    }

    qDebug() << "Auth request to me and I am " << this;
    _authAttempts++;
    MirallConfigFile cfgFile( configHandle );
    qDebug() << "Authenticating request for " << reply->url();
    if( reply->url().toString().startsWith( cfgFile.ownCloudUrl( _connection, true )) ) {
        auth->setUser( CredentialStore::instance()->user() ); //_connection ) );
        auth->setPassword( CredentialStore::instance()->password() ); // _connection ));
    } else {
        qDebug() << "WRN: attempt to authenticate to different url - attempt " <<_authAttempts;
    }
    if( _authAttempts > 10 ) {
        qDebug() << "Too many attempts to authenticate. Stop request.";
        reply->close();
    }

}

QString ownCloudInfo::configHandle(QNetworkReply *reply)
{
    QString configHandle;
    if( _configHandleMap.contains(reply) ) {
        configHandle = _configHandleMap[reply];
    }
    return configHandle;
}

QUrl ownCloudInfo::redirectUrl(const QUrl& possibleRedirectUrl,
                               const QUrl& oldRedirectUrl) const {
    QUrl redirectUrl;
    /*
     * Check if the URL is empty and
     * that we aren't being fooled into a infinite redirect loop.
     */
    if(!possibleRedirectUrl.isEmpty() &&
       possibleRedirectUrl != oldRedirectUrl) {
        redirectUrl = possibleRedirectUrl;
    }
    return redirectUrl;
}

//
// There have been problems with the finish-signal coming from the networkmanager.
// To avoid that, the reply-signals were connected and the data is taken from the
// sender() method.
//
void ownCloudInfo::slotReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    if( ! reply ) {
        qDebug() << "ownCloudInfo: Reply empty!";
        return;
    }

    // Detect redirect url
    QVariant possibleRedirUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    /* We'll deduct if the redirection is valid in the redirectUrl function */
    _urlRedirectedTo = redirectUrl( possibleRedirUrl.toUrl(),
                                    _urlRedirectedTo );

    if(!_urlRedirectedTo.isEmpty()) {
        QString configHandle;

        qDebug() << "Redirected to " << possibleRedirUrl;

        // We'll do another request to the redirection url.
        // an empty config handle is ok for the default config.
        if( _configHandleMap.contains(reply) ) {
            configHandle = _configHandleMap[reply];
            qDebug() << "Redirect: Have a custom config handle: " << configHandle;
        }

        QString path = _directories[reply];
        qDebug() << "This path was redirected: " << path;

        MirallConfigFile cfgFile( configHandle );
        QString newUrl = _urlRedirectedTo.toString();
        if( newUrl.endsWith( path )) {
            // cut off the trailing path
            newUrl.chop( path.length() );
            cfgFile.setOwnCloudUrl( _connection, newUrl );

            qDebug() << "Update the config file url to " << newUrl;
            getRequest( path, false ); // FIXME: Redirect for webdav!
            reply->deleteLater();
            return;
        } else {
            qDebug() << "WRN: Path is not part of the redirect URL. NO redirect.";
        }
    }
    _urlRedirectedTo.clear();

    // TODO: check if this is always the correct encoding
    const QString version = QString::fromUtf8( reply->readAll() );
    const QString url = reply->url().toString();
    QString plainUrl(url);
    plainUrl.remove( QLatin1String("/status.php"));

    QString info( version );

    if( url.endsWith( QLatin1String("status.php")) ) {
        // it was a call to status.php
        if( reply->error() == QNetworkReply::NoError && info.isEmpty() ) {
            // This seems to be a bit strange behaviour of QNetworkAccessManager.
            // It calls the finised slot multiple times but only the first read wins.
            // That happend when the code connected the finished signal of the manager.
            // It did not happen when the code connected to the reply finish signal.
            qDebug() << "WRN: NetworkReply with not content but also no error! " << reply;
            reply->deleteLater();
            return;
        }
        qDebug() << "status.php returns: " << info << " " << reply->error() << " Reply: " << reply;
        if( info.contains(QLatin1String("installed"))
                && info.contains(QLatin1String("version"))
                && info.contains(QLatin1String("versionstring")) ) {
            info.remove(0,1); // remove first char which is a "{"
            info.remove(-1,1); // remove the last char which is a "}"
            QStringList li = info.split( QLatin1Char(',') );

            QString versionStr;
            QString version;
            QString edition;

            foreach ( const QString& infoString, li ) {
                QStringList touple = infoString.split( QLatin1Char(':'));
                QString key = touple[0];
                key.remove(QLatin1Char('"'));
                QString val = touple[1];
                val.remove(QLatin1Char('"'));

                if( key == QLatin1String("versionstring") ) {
                    // get the versionstring out.
                    versionStr = val;
                } else if( key == QLatin1String( "version") ) {
                    // get version out
                    version = val;
                } else if( key == QLatin1String( "edition") ) {
                    // get version out
                    edition = val;
                } else {
                    qDebug() << "Unknown info from ownCloud status.php: "<< key << "=" << val;
                }
            }
            emit ownCloudInfoFound( plainUrl, versionStr, version, edition );
        } else {
            qDebug() << "No proper answer on " << url;

            emit noOwncloudFound( reply );
        }
    } else {
        // it was a general GET request.
        QString dir(QLatin1String("unknown"));
        if( _directories.contains(reply) ) {
            dir = _directories[reply];
            _directories.remove(reply);
        }

        emit ownCloudDirExists( dir, reply );
    }
    if( _configHandleMap.contains(reply)) {
        _configHandleMap.remove(reply);
    }
    reply->deleteLater();
}

void ownCloudInfo::resetSSLUntrust()
{
    _certsUntrusted = false;
}

void ownCloudInfo::setCertsUntrusted(bool donttrust)
{
    _certsUntrusted = donttrust;
}

bool ownCloudInfo::certsUntrusted()
{
    return _certsUntrusted;
}

void ownCloudInfo::slotError( QNetworkReply::NetworkError err)
{
  qDebug() << "ownCloudInfo Network Error: " << err;
}

// ============================================================================
void ownCloudInfo::setupHeaders( QNetworkRequest & req, quint64 size )
{
    MirallConfigFile cfgFile(_configHandle );

    QUrl url( cfgFile.ownCloudUrl( QString::null, false ) );
    qDebug() << "Setting up host header: " << url.host();
    req.setRawHeader( QByteArray("Host"), url.host().toUtf8() );
    req.setRawHeader( QByteArray("User-Agent"), QString::fromLatin1("mirall-%1")
                      .arg(QLatin1String(MIRALL_STRINGIFY(MIRALL_VERSION))).toAscii());
    req.setRawHeader( QByteArray("Authorization"), CredentialStore::instance()->basicAuthHeader() );

    if (size) {
        req.setHeader( QNetworkRequest::ContentLengthHeader, size);
        req.setHeader( QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml; charset=utf-8"));
    }
}

#if QT46_IMPL
#else
QNetworkReply* ownCloudInfo::davRequest(const QString& reqVerb,  QNetworkRequest& req, QByteArray *data)
{
    setupHeaders(req, quint64(data ? data->size() : 0));
    if( data ) {
        QBuffer iobuf( data );
        return _manager->sendCustomRequest(req, reqVerb.toUtf8(), &iobuf );
    } else {
        return _manager->sendCustomRequest(req, reqVerb.toUtf8(), 0 );
    }
}
#endif
}


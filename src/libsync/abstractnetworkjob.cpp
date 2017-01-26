/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QStringList>
#include <QStack>
#include <QTimer>
#include <QMutex>
#include <QDebug>
#include <QCoreApplication>

#include "json.h"

#include "networkjobs.h"
#include "account.h"
#include "owncloudpropagator.h"

#include "creds/abstractcredentials.h"

Q_DECLARE_METATYPE(QTimer*)

namespace OCC {


AbstractNetworkJob::AbstractNetworkJob(AccountPtr account, const QString &path, QObject *parent)
    : QObject(parent)
    , _timedout(false)
    , _followRedirects(true)
    , _account(account)
    , _ignoreCredentialFailure(false)
    , _reply(0)
    , _path(path)
    , _redirectCount(0)
{
    _timer.setSingleShot(true);
    _timer.setInterval(OwncloudPropagator::httpTimeout() * 1000); // default to 5 minutes.
    connect(&_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));

    connect(this, SIGNAL(networkActivity()), SLOT(resetTimeout()));

    // Network activity on the propagator jobs (GET/PUT) keeps all requests alive.
    // This is a workaround for OC instances which only support one
    // parallel up and download
    if (_account) {
        connect(_account.data(), SIGNAL(propagatorNetworkActivity()), SLOT(resetTimeout()));
    }
}

void AbstractNetworkJob::setReply(QNetworkReply *reply)
{
    if (reply)
        reply->setProperty("doNotHandleAuth", true);

    QNetworkReply *old = _reply;
    _reply = reply;
    delete old;
}

void AbstractNetworkJob::setTimeout(qint64 msec)
{
    //qDebug() << Q_FUNC_INFO << msec;

    _timer.start(msec);
}

void AbstractNetworkJob::resetTimeout()
{
    qint64 interval = _timer.interval();
    _timer.stop();
    _timer.start(interval);
}

void AbstractNetworkJob::setIgnoreCredentialFailure(bool ignore)
{
    _ignoreCredentialFailure = ignore;
}

void AbstractNetworkJob::setPath(const QString &path)
{
    _path = path;
}

void AbstractNetworkJob::setupConnections(QNetworkReply *reply)
{
    connect(reply, SIGNAL(finished()), SLOT(slotFinished()));
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
    connect(reply, SIGNAL(encrypted()), SIGNAL(networkActivity()));
#endif
    connect(reply->manager(), SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(metaDataChanged()), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(uploadProgress(qint64,qint64)), SIGNAL(networkActivity()));
}

QNetworkReply* AbstractNetworkJob::addTimer(QNetworkReply *reply)
{
    reply->setProperty("timer", QVariant::fromValue(&_timer));
    return reply;
}

QNetworkReply* AbstractNetworkJob::davRequest(const QByteArray &verb, const QString &relPath,
                                              QNetworkRequest req, QIODevice *data)
{
    return addTimer(_account->davRequest(verb, relPath, req, data));
}

QNetworkReply *AbstractNetworkJob::davRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    return addTimer(_account->davRequest(verb, url, req, data));
}

QNetworkReply* AbstractNetworkJob::getRequest(const QString &relPath)
{
    return addTimer(_account->getRequest(relPath));
}

QNetworkReply *AbstractNetworkJob::getRequest(const QUrl &url)
{
    return addTimer(_account->getRequest(url));
}

QNetworkReply *AbstractNetworkJob::headRequest(const QString &relPath)
{
    return addTimer(_account->headRequest(relPath));
}

QNetworkReply *AbstractNetworkJob::headRequest(const QUrl &url)
{
    return addTimer(_account->headRequest(url));
}

QNetworkReply *AbstractNetworkJob::deleteRequest(const QUrl &url)
{
    return addTimer(_account->deleteRequest(url));
}

void AbstractNetworkJob::slotFinished()
{
    _timer.stop();

    if( _reply->error() == QNetworkReply::SslHandshakeFailedError ) {
        qDebug() << "SslHandshakeFailedError: " << reply()->errorString() << " : can be caused by a webserver wanting SSL client certificates";
    }

    if( _reply->error() != QNetworkReply::NoError ) {
        qDebug() << Q_FUNC_INFO << _reply->error() << _reply->errorString()
                 << _reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (_reply->error() == QNetworkReply::ProxyAuthenticationRequiredError) {
            qDebug() << Q_FUNC_INFO << _reply->rawHeader("Proxy-Authenticate");
        }
        emit networkError(_reply);
    }

    // get the Date timestamp from reply
    _responseTimestamp = _reply->rawHeader("Date");

    if (_followRedirects) {
        // ### the qWarnings here should be exported via displayErrors() so they
        // ### can be presented to the user if the job executor has a GUI
        QUrl requestedUrl = reply()->request().url();
        QUrl redirectUrl = reply()->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (!redirectUrl.isEmpty()) {
            _redirectCount++;
            if (requestedUrl.scheme() == QLatin1String("https") &&
                    redirectUrl.scheme() == QLatin1String("http")) {
                qWarning() << this << "HTTPS->HTTP downgrade detected!";
            } else if (requestedUrl == redirectUrl || _redirectCount >= maxRedirects()) {
                qWarning() << this << "Redirect loop detected!";
            } else {
                resetTimeout();
                setReply(getRequest(redirectUrl));
                setupConnections(reply());
                return;
            }
        }
    }

    AbstractCredentials *creds = _account->credentials();
    if (!creds->stillValid(_reply) && ! _ignoreCredentialFailure) {
        _account->handleInvalidCredentials();
    }

    bool discard = finished();
    if (discard) {
        deleteLater();
    }
}

QByteArray AbstractNetworkJob::responseTimestamp()
{
    return _responseTimestamp;
}

AbstractNetworkJob::~AbstractNetworkJob()
{
    setReply(0);
}

void AbstractNetworkJob::start()
{
    _timer.start();

    const QUrl url = account()->url();
    const QString displayUrl = QString( "%1://%2%3").arg(url.scheme()).arg(url.host()).arg(url.path());

    QString parentMetaObjectName = parent() ? parent()->metaObject()->className() : "";
    qDebug() << "!!!" << metaObject()->className() << "created for" << displayUrl << "+" << path() << parentMetaObjectName;
}

void AbstractNetworkJob::slotTimeout()
{
    _timedout = true;
    if (reply()) {
        qDebug() << Q_FUNC_INFO << this << "Timeout" << reply()->request().url();
        reply()->abort();
    } else {
        qDebug() << Q_FUNC_INFO << this << "Timeout reply was NULL";
        deleteLater();
    }
}


NetworkJobTimeoutPauser::NetworkJobTimeoutPauser(QNetworkReply *reply)
{
    _timer = reply->property("timer").value<QTimer*>();
    if(!_timer.isNull()) {
        _timer->stop();
    }
}

NetworkJobTimeoutPauser::~NetworkJobTimeoutPauser()
{
    if(!_timer.isNull()) {
        _timer->start();
    }
}

QString extractErrorMessage(const QByteArray& errorResponse)
{
    QXmlStreamReader reader(errorResponse);
    reader.readNextStartElement();
    if (reader.name() != "error") {
        return QString::null;
    }

    QString exception;
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNextStartElement();
        if (reader.name() == QLatin1String("message")) {
            QString message = reader.readElementText();
            if (!message.isEmpty()) {
                return message;
            }
        } else if (reader.name() == QLatin1String("exception")) {
            exception = reader.readElementText();
        }

    }
    // Fallback, if message could not be found
    return exception;
}

QString errorMessage(const QString& baseError, const QByteArray& body)
{
    QString msg = baseError;
    QString extra = extractErrorMessage(body);
    if (!extra.isEmpty()) {
        msg += QString::fromLatin1(" (%1)").arg(extra);
    }
    return msg;
}

} // namespace OCC

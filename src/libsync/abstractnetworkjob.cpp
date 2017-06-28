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

#include <QLoggingCategory>
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
#include <QCoreApplication>

#include "networkjobs.h"
#include "account.h"
#include "owncloudpropagator.h"

#include "creds/abstractcredentials.h"

Q_DECLARE_METATYPE(QTimer *)

namespace OCC {

Q_LOGGING_CATEGORY(lcNetworkJob, "sync.networkjob", QtInfoMsg)

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

void AbstractNetworkJob::setFollowRedirects(bool follow)
{
    _followRedirects = follow;
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
    connect(reply->manager(), SIGNAL(proxyAuthenticationRequired(QNetworkProxy, QAuthenticator *)), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(metaDataChanged()), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(uploadProgress(qint64, qint64)), SIGNAL(networkActivity()));
}

QNetworkReply *AbstractNetworkJob::addTimer(QNetworkReply *reply)
{
    reply->setProperty("timer", QVariant::fromValue(&_timer));
    return reply;
}

QNetworkReply *AbstractNetworkJob::sendRequest(const QByteArray &verb, const QUrl &url,
    QNetworkRequest req, QIODevice *requestBody)
{
    auto reply = _account->sendRequest(verb, url, req, requestBody);
    _requestBody = requestBody;
    if (_requestBody) {
        _requestBody->setParent(reply);
    }
    addTimer(reply);
    setReply(reply);
    setupConnections(reply);
    return reply;
}

QUrl AbstractNetworkJob::makeAccountUrl(const QString &relativePath) const
{
    return Utility::concatUrlPath(_account->url(), relativePath);
}

QUrl AbstractNetworkJob::makeDavUrl(const QString &relativePath) const
{
    return Utility::concatUrlPath(_account->davUrl(), relativePath);
}

void AbstractNetworkJob::slotFinished()
{
    _timer.stop();

    if (_reply->error() == QNetworkReply::SslHandshakeFailedError) {
        qCWarning(lcNetworkJob) << "SslHandshakeFailedError: " << errorString() << " : can be caused by a webserver wanting SSL client certificates";
    }

    if (_reply->error() != QNetworkReply::NoError) {
        if (!_ignoreCredentialFailure || _reply->error() != QNetworkReply::AuthenticationRequiredError) {
            qCWarning(lcNetworkJob) << _reply->error() << errorString()
                                    << _reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            if (_reply->error() == QNetworkReply::ProxyAuthenticationRequiredError) {
                qCWarning(lcNetworkJob) << _reply->rawHeader("Proxy-Authenticate");
            }
        }
        emit networkError(_reply);
    }

    // get the Date timestamp from reply
    _responseTimestamp = _reply->rawHeader("Date");

    QUrl requestedUrl = reply()->request().url();
    QUrl redirectUrl = reply()->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (_followRedirects && !redirectUrl.isEmpty()) {
        _redirectCount++;

        // ### some of the qWarnings here should be exported via displayErrors() so they
        // ### can be presented to the user if the job executor has a GUI
        QByteArray verb = requestVerb(*reply());
        if (requestedUrl.scheme() == QLatin1String("https") && redirectUrl.scheme() == QLatin1String("http")) {
            qCWarning(lcNetworkJob) << this << "HTTPS->HTTP downgrade detected!";
        } else if (requestedUrl == redirectUrl || _redirectCount >= maxRedirects()) {
            qCWarning(lcNetworkJob) << this << "Redirect loop detected!";
        } else if (_requestBody && _requestBody->isSequential()) {
            qCWarning(lcNetworkJob) << this << "cannot redirect request with sequential body";
        } else if (verb.isEmpty()) {
            qCWarning(lcNetworkJob) << this << "cannot redirect request: could not detect original verb";
        } else {
            // Create the redirected request and send it
            qCInfo(lcNetworkJob) << "Redirecting" << verb << requestedUrl << redirectUrl;
            resetTimeout();
            if (_requestBody) {
                _requestBody->seek(0);
            }
            sendRequest(
                verb,
                redirectUrl,
                reply()->request(),
                _requestBody);
            return;
        }
    }

    AbstractCredentials *creds = _account->credentials();
    if (!creds->stillValid(_reply) && !_ignoreCredentialFailure) {
        _account->handleInvalidCredentials();
    }

    bool discard = finished();
    if (discard) {
        qCDebug(lcNetworkJob) << "Network job" << metaObject()->className() << "finished for" << path();
        deleteLater();
    }
}

QByteArray AbstractNetworkJob::responseTimestamp()
{
    return _responseTimestamp;
}

QString AbstractNetworkJob::errorString() const
{
    if (_timedout) {
        return tr("Connection timed out");
    } else if (!reply()) {
        return tr("Unknown error: network reply was deleted");
    } else if (reply()->hasRawHeader("OC-ErrorString")) {
        return reply()->rawHeader("OC-ErrorString");
    } else {
        return networkReplyErrorString(*reply());
    }
}

QString AbstractNetworkJob::errorStringParsingBody(QByteArray *body)
{
    QString base = errorString();
    if (base.isEmpty() || !reply()) {
        return QString();
    }

    QByteArray replyBody = reply()->readAll();
    if (body) {
        *body = replyBody;
    }

    QString extra = extractErrorMessage(replyBody);
    // Don't append the XML error message to a OC-ErrorString message.
    if (!extra.isEmpty() && !reply()->hasRawHeader("OC-ErrorString")) {
        return QString::fromLatin1("%1 (%2)").arg(base, extra);
    }

    return base;
}

AbstractNetworkJob::~AbstractNetworkJob()
{
    setReply(0);
}

void AbstractNetworkJob::start()
{
    _timer.start();

    const QUrl url = account()->url();
    const QString displayUrl = QString("%1://%2%3").arg(url.scheme()).arg(url.host()).arg(url.path());

    QString parentMetaObjectName = parent() ? parent()->metaObject()->className() : "";
    qCInfo(lcNetworkJob) << metaObject()->className() << "created for" << displayUrl << "+" << path() << parentMetaObjectName;
}

void AbstractNetworkJob::slotTimeout()
{
    _timedout = true;
    qCWarning(lcNetworkJob) << "Network job timeout" << (reply() ? reply()->request().url() : path());
    onTimedOut();
}

void AbstractNetworkJob::onTimedOut()
{
    if (reply()) {
        reply()->abort();
    } else {
        deleteLater();
    }
}


NetworkJobTimeoutPauser::NetworkJobTimeoutPauser(QNetworkReply *reply)
{
    _timer = reply->property("timer").value<QTimer *>();
    if (!_timer.isNull()) {
        _timer->stop();
    }
}

NetworkJobTimeoutPauser::~NetworkJobTimeoutPauser()
{
    if (!_timer.isNull()) {
        _timer->start();
    }
}

QString extractErrorMessage(const QByteArray &errorResponse)
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

QString errorMessage(const QString &baseError, const QByteArray &body)
{
    QString msg = baseError;
    QString extra = extractErrorMessage(body);
    if (!extra.isEmpty()) {
        msg += QString::fromLatin1(" (%1)").arg(extra);
    }
    return msg;
}

QByteArray requestVerb(const QNetworkReply &reply)
{
    switch (reply.operation()) {
    case QNetworkAccessManager::HeadOperation:
        return "HEAD";
    case QNetworkAccessManager::GetOperation:
        return "GET";
    case QNetworkAccessManager::PutOperation:
        return "PUT";
    case QNetworkAccessManager::PostOperation:
        return "POST";
    case QNetworkAccessManager::DeleteOperation:
        return "DELETE";
    case QNetworkAccessManager::CustomOperation:
        return reply.request().attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
    case QNetworkAccessManager::UnknownOperation:
        break;
    }
    return QByteArray();
}

QString networkReplyErrorString(const QNetworkReply &reply)
{
    QString base = reply.errorString();
    int httpStatus = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString httpReason = reply.attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

    // Only adjust HTTP error messages of the expected format.
    if (httpReason.isEmpty() || httpStatus == 0 || !base.contains(httpReason)) {
        return base;
    }

    return AbstractNetworkJob::tr("Server replied \"%1 %2\" to \"%3 %4\"").arg(QString::number(httpStatus), httpReason, requestVerb(reply),
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        reply.request().url().toString()
#else
        reply.request().url().toDisplayString()
#endif
            );
}

} // namespace OCC

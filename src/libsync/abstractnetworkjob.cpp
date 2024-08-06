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
#include <QAuthenticator>
#include <QMetaEnum>
#include <QRegularExpression>

#include "common/asserts.h"
#include "networkjobs.h"
#include "account.h"
#include "owncloudpropagator.h"
#include "httplogger.h"

#include "creds/abstractcredentials.h"

Q_DECLARE_METATYPE(QTimer *)

namespace OCC {

Q_LOGGING_CATEGORY(lcNetworkJob, "nextcloud.sync.networkjob", QtInfoMsg)

// If not set, it is overwritten by the Application constructor with the value from the config
int AbstractNetworkJob::httpTimeout = qEnvironmentVariableIntValue("OWNCLOUD_TIMEOUT");

AbstractNetworkJob::AbstractNetworkJob(const AccountPtr &account, const QString &path, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _reply(nullptr)
    , _path(path)
{
    // Since we hold a QSharedPointer to the account, this makes no sense. (issue #6893)
    ASSERT(account != parent);

    _timer.setSingleShot(true);
    _timer.setInterval((httpTimeout ? httpTimeout : 300) * 1000); // default to 5 minutes.
    connect(&_timer, &QTimer::timeout, this, &AbstractNetworkJob::slotTimeout);

    connect(this, &AbstractNetworkJob::networkActivity, this, &AbstractNetworkJob::resetTimeout);

    // Network activity on the propagator jobs (GET/PUT) keeps all requests alive.
    // This is a workaround for OC instances which only support one
    // parallel up and download
    if (_account) {
        connect(_account.data(), &Account::propagatorNetworkActivity, this, &AbstractNetworkJob::resetTimeout);
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
    connect(reply, &QNetworkReply::finished, this, &AbstractNetworkJob::slotFinished);
    connect(reply, &QNetworkReply::encrypted, this, &AbstractNetworkJob::networkActivity);
    connect(reply->manager(), &QNetworkAccessManager::proxyAuthenticationRequired, this, &AbstractNetworkJob::networkActivity);
    connect(reply, &QNetworkReply::sslErrors, this, &AbstractNetworkJob::networkActivity);
    connect(reply, &QNetworkReply::metaDataChanged, this, &AbstractNetworkJob::networkActivity);
    connect(reply, &QNetworkReply::downloadProgress, this, &AbstractNetworkJob::networkActivity);
    connect(reply, &QNetworkReply::uploadProgress, this, &AbstractNetworkJob::networkActivity);
    connect(reply, &QNetworkReply::redirected, this, [reply, this] (const QUrl &url) { emit redirected(reply, url, 0);});
}

QNetworkReply *AbstractNetworkJob::addTimer(QNetworkReply *reply)
{
    reply->setProperty("timer", QVariant::fromValue(&_timer));
    return reply;
}

QNetworkReply *AbstractNetworkJob::sendRequest(const QByteArray &verb, const QUrl &url,
    QNetworkRequest req, QIODevice *requestBody)
{
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto reply = _account->sendRawRequest(verb, url, req, requestBody);
    _requestBody = requestBody;
    if (_requestBody) {
        _requestBody->setParent(reply);
    }
    adoptRequest(reply);
    return reply;
}

QNetworkReply *AbstractNetworkJob::sendRequest(const QByteArray &verb, const QUrl &url,
    QNetworkRequest req, const QByteArray &requestBody)
{
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto reply = _account->sendRawRequest(verb, url, req, requestBody);
    _requestBody = nullptr;
    adoptRequest(reply);
    return reply;
}

QNetworkReply *AbstractNetworkJob::sendRequest(const QByteArray &verb,
                                               const QUrl &url,
                                               QNetworkRequest req,
                                               QHttpMultiPart *requestBody)
{
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto reply = _account->sendRawRequest(verb, url, req, requestBody);
    _requestBody = nullptr;
    adoptRequest(reply);
    return reply;
}

void AbstractNetworkJob::adoptRequest(QNetworkReply *reply)
{
    addTimer(reply);
    setReply(reply);
    setupConnections(reply);
    newReplyHook(reply);
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

    // TODO: this could be removed with Qt6
    const auto maxHttp2Resends = 3;
    if (const auto verb = HttpLogger::requestVerb(*reply());
        _reply->error() == QNetworkReply::ContentReSendError
        && _reply->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool()) {

        if ((_requestBody && !_requestBody->isSequential()) || verb.isEmpty()) {
            qCWarning(lcNetworkJob) << "Can't resend HTTP2 request, verb or body not suitable"
                                    << _reply->request().url() << verb << _requestBody;
        } else if (_http2ResendCount >= maxHttp2Resends) {
            qCWarning(lcNetworkJob) << "Not resending HTTP2 request, number of resends exhausted"
                                    << _reply->request().url() << _http2ResendCount;
        } else {
            qCInfo(lcNetworkJob) << "HTTP2 resending" << _reply->request().url();
            _http2ResendCount++;

            resetTimeout();
            if (_requestBody) {
                if(!_requestBody->isOpen())
                   _requestBody->open(QIODevice::ReadOnly);
                _requestBody->seek(0);
            }
            sendRequest(
                verb,
                _reply->request().url(),
                _reply->request(),
                _requestBody);
            return;
        }
    }

    if (_reply->error() != QNetworkReply::NoError) {
        if (_account->credentials()->retryIfNeeded(this)) {
            return;
        }

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

    if (auto *creds = _account->credentials();!creds->stillValid(_reply) && !_ignoreCredentialFailure) {
        _account->handleInvalidCredentials();
    }

    if (finished()) {
        qCDebug(lcNetworkJob) << "Network job" << metaObject()->className() << "finished for" << path();
        deleteLater();
    }
}

QByteArray AbstractNetworkJob::responseTimestamp()
{
    ASSERT(!_responseTimestamp.isEmpty());
    return _responseTimestamp;
}

QByteArray AbstractNetworkJob::requestId()
{
    return  _reply ? _reply->request().rawHeader("X-Request-ID") : QByteArray();
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
    const auto base = errorString();
    if (base.isEmpty() || !reply()) {
        return QString();
    }

    const auto replyBody = reply()->readAll();
    if (body) {
        *body = replyBody;
    }

    const auto extra = extractErrorMessage(replyBody);
    // Don't append the XML error message to a OC-ErrorString message.
    if (!extra.isEmpty() && !reply()->hasRawHeader("OC-ErrorString")) {
        return extra;
    }

    return base;
}

QString AbstractNetworkJob::errorStringParsingBodyException(const QByteArray &body) const
{
    return extractException(body);
}

AbstractNetworkJob::~AbstractNetworkJob()
{
    setReply(nullptr);
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

QString AbstractNetworkJob::replyStatusString() {
    Q_ASSERT(reply());
    if (reply()->error() == QNetworkReply::NoError) {
        return QLatin1String("OK");
    } else {
        QString enumStr = QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(static_cast<int>(reply()->error()));
        return QStringLiteral("%1 %2").arg(enumStr, errorString());
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
        return QString();
    }

    QString exception;
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNextStartElement();
        if (reader.name() == QLatin1String("message")) {
            const auto message = reader.readElementText();
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

QString extractException(const QByteArray &errorResponse)
{
    QXmlStreamReader reader(errorResponse);
    reader.readNextStartElement();
    if (reader.name() != QLatin1String("error")) {
        return {};
    }

    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNextStartElement();
        if (reader.name() == QLatin1String("exception")) {
            return reader.readElementText();
        }
    }
    return {};
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

QString networkReplyErrorString(const QNetworkReply &reply)
{
    const auto base = reply.errorString();
    const auto httpStatus = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto httpReason = reply.attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

    // Only adjust HTTP error messages of the expected format.
    if (httpReason.isEmpty() || httpStatus == 0 || !base.contains(httpReason)) {
        return base;
    }

    const auto displayString = reply.request().url().toDisplayString();
    const auto requestVerb = HttpLogger::requestVerb(reply);

    return AbstractNetworkJob::tr(R"(Server replied "%1 %2" to "%3 %4")").arg(QString::number(httpStatus),
                                                                                httpReason,
                                                                                requestVerb,
                                                                                displayString);
}

void AbstractNetworkJob::retry()
{
    ENFORCE(_reply);
    auto req = _reply->request();
    QUrl requestedUrl = req.url();
    QByteArray verb = HttpLogger::requestVerb(*_reply);
    qCInfo(lcNetworkJob) << "Restarting" << verb << requestedUrl;
    resetTimeout();
    if (_requestBody) {
        _requestBody->seek(0);
    }
    // The cookie will be added automatically, we don't want AccessManager::createRequest to duplicate them
    req.setRawHeader("cookie", QByteArray());
    sendRequest(verb, requestedUrl, req, _requestBody);
}

} // namespace OCC

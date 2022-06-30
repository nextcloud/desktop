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

namespace {
const int MaxRetryCount = 5;
}


namespace OCC {

Q_LOGGING_CATEGORY(lcNetworkJob, "sync.networkjob", QtInfoMsg)

// If not set, it is overwritten by the Application constructor with the value from the config
int AbstractNetworkJob::httpTimeout = qEnvironmentVariableIntValue("OWNCLOUD_TIMEOUT");

AbstractNetworkJob::AbstractNetworkJob(AccountPtr account, const QString &path, QObject *parent)
    : QObject(parent)
    , _timedout(false)
    , _account(account)
    , _ignoreCredentialFailure(false)
    , _reply(nullptr)
    , _path(path)
{
    // Since we hold a QSharedPointer to the account, this makes no sense. (issue #6893)
    OC_ASSERT(account != parent);

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

void AbstractNetworkJob::setPath(const QString &path)
{
    _path = path;
}

QNetworkReply *AbstractNetworkJob::reply() const
{
    Q_ASSERT(_reply);
    return _reply;
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
}

QNetworkReply *AbstractNetworkJob::addTimer(QNetworkReply *reply)
{
    reply->setProperty("timer", QVariant::fromValue(&_timer));
    return reply;
}

bool AbstractNetworkJob::isAuthenticationJob() const
{
    return _isAuthenticationJob;
}

void AbstractNetworkJob::setAuthenticationJob(bool b)
{
    _isAuthenticationJob = b;
}

bool AbstractNetworkJob::needsRetry() const
{
    if (isAuthenticationJob()) {
        qCDebug(lcNetworkJob) << "Not Retry auth job" << this << url();
        return false;
    }
    if (retryCount() >= MaxRetryCount) {
        qCDebug(lcNetworkJob) << "Not Retry too many retries" << retryCount() << this << url();
        return false;
    }

    if (auto reply = this->reply()) {
        if (!reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isNull()) {
            return true;
        }
        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
                return true;
            }
        }
    }
    return false;
}


void AbstractNetworkJob::sendRequest(const QByteArray &verb, const QUrl &url,
    const QNetworkRequest &req, QIODevice *requestBody)
{
    _verb = verb;
    _request = req;
    _request.setUrl(url);
    _request.setPriority(_priority);
    _requestBody = requestBody;
    if (!isAuthenticationJob() && _account->jobQueue()->enqueue(this)) {
        return;
    }
    auto reply = _account->sendRawRequest(verb, url, req, requestBody);
    if (_requestBody) {
        _requestBody->setParent(reply);
    }
    adoptRequest(reply);
}

void AbstractNetworkJob::adoptRequest(QNetworkReply *reply)
{
    addTimer(reply);
    setReply(reply);
    setupConnections(reply);
    newReplyHook(reply);
    _request = reply->request();
}

QUrl AbstractNetworkJob::makeAccountUrl(const QString &relativePath) const
{
    return Utility::concatUrlPath(_account->url(), relativePath);
}

QUrl AbstractNetworkJob::makeDavUrl(const QString &relativePath) const
{
    // ensure we always used the remote folder
    return Utility::concatUrlPath(_account->davUrl(), OC_ENSURE(relativePath.startsWith(QLatin1Char('/'))) ? relativePath : QLatin1Char('/') + relativePath);
}

void AbstractNetworkJob::slotFinished()
{
    _timer.stop();

    if (_reply->error() == QNetworkReply::SslHandshakeFailedError) {
        qCWarning(lcNetworkJob) << "SslHandshakeFailedError: " << errorString() << " : can be caused by a webserver wanting SSL client certificates";
    }
    // Qt doesn't yet transparently resend HTTP2 requests, do so here
    const auto maxHttp2Resends = 3;
    QByteArray verb = HttpLogger::requestVerb(*reply());
    if (_reply->error() == QNetworkReply::ContentReSendError
        && _reply->attribute(QNetworkRequest::HTTP2WasUsedAttribute).toBool()) {

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
        if (_account->jobQueue()->retry(this)) {
            qCDebug(lcNetworkJob) << "Queuing: " << _reply->url() << " for retry";
            return;
        }

        if (!_ignoreCredentialFailure || _reply->error() != QNetworkReply::AuthenticationRequiredError) {
            qCWarning(lcNetworkJob) << this << _reply->error() << errorString()
                                    << _reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (_reply->error() == QNetworkReply::ProxyAuthenticationRequiredError) {
                qCWarning(lcNetworkJob) << _reply->rawHeader("Proxy-Authenticate");
            }
        }
        emit networkError(_reply);
    }

    // get the Date timestamp from reply
    _responseTimestamp = _reply->rawHeader("Date");

    if (!_account->credentials()->stillValid(_reply) && !_ignoreCredentialFailure) {
        Q_EMIT _account->invalidCredentials();
    }
    if (!reply()->attribute(QNetworkRequest::RedirectionTargetAttribute).isNull() && !(isAuthenticationJob() || reply()->request().hasRawHeader(QByteArrayLiteral("OC-Connection-Validator")))) {
        Q_EMIT _account->unknownConnectionState();
        qCWarning(lcNetworkJob) << this << "Unsupported redirect on" << _reply->url().toString() << "to" << reply()->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();
        Q_EMIT networkError(_reply);
        if (_account->jobQueue()->retry(this)) {
            qCWarning(lcNetworkJob) << "Retry Nr:" << _retryCount << _reply->url();
            return;
        } else {
            qCWarning(lcNetworkJob) << "Don't retry:" << _reply->url();
        }
    }

    bool discard = finished();
    if (discard) {
        qCDebug(lcNetworkJob) << "Network job finished" << this;
        deleteLater();
    }
}

QByteArray AbstractNetworkJob::responseTimestamp()
{
    OC_ASSERT(!_responseTimestamp.isEmpty());
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
        return QString::fromUtf8(reply()->rawHeader("OC-ErrorString"));
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
        return QStringLiteral("%1 (%2)").arg(base, extra);
    }

    return base;
}

AbstractNetworkJob::~AbstractNetworkJob()
{
    setReply(nullptr);
}

void AbstractNetworkJob::start()
{
    _timer.start();
    qCInfo(lcNetworkJob) << "Created" << this << "for" << parent();
}

void AbstractNetworkJob::slotTimeout()
{
    _timedout = true;
    qCWarning(lcNetworkJob) << "Network job" << this << "timeout";
    onTimedOut();
}

void AbstractNetworkJob::onTimedOut()
{
    abort();
}

QString AbstractNetworkJob::replyStatusString() {
    Q_ASSERT(reply());
    if (reply()->error() == QNetworkReply::NoError) {
        return QStringLiteral("OK");
    } else {
        const QString enumStr = QString::fromUtf8(QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(static_cast<int>(reply()->error())));
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
    if (reader.name() != QLatin1String("error")) {
        return QString();
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
        msg += QStringLiteral(" (%1)").arg(extra);
    }
    return msg;
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

    return AbstractNetworkJob::tr("Server replied \"%1 %2\" to \"%3 %4\"").arg(QString::number(httpStatus), httpReason, QString::fromLatin1(HttpLogger::requestVerb(reply)), reply.request().url().toDisplayString());
}

void AbstractNetworkJob::retry()
{
    OC_ENFORCE(!_verb.isEmpty());
    _retryCount++;
    qCInfo(lcNetworkJob) << "Restarting" << _verb << _request.url() << "for the" << _retryCount << "time";
    resetTimeout();
    if (_requestBody) {
        _requestBody->seek(0);
    }
    sendRequest(_verb, _request.url(), _request, _requestBody);
}

void AbstractNetworkJob::abort()
{
    if (_reply) {
        _reply->abort();
    } else {
        deleteLater();
    }
}

void AbstractNetworkJob::setPriority(QNetworkRequest::Priority priority)
{
    _priority = priority;
}

QNetworkRequest::Priority AbstractNetworkJob::priority() const
{
    return _priority;
}

} // namespace OCC

QDebug operator<<(QDebug debug, const OCC::AbstractNetworkJob *job)
{
    QDebugStateSaver saver(debug);
    debug.setAutoInsertSpaces(false);
    debug << job->metaObject()->className() << "(" << job->url().toDisplayString();
    if (auto reply = job->_reply) {
        debug << ", " << reply->request().rawHeader("Original-Request-ID")
              << ", " << reply->request().rawHeader("X-Request-ID");

        const auto errorString = reply->rawHeader(QByteArrayLiteral("OC-ErrorString"));
        if (!errorString.isEmpty()) {
            debug << ", " << errorString;
        }
        if (reply->error() != QNetworkReply::NoError) {
            debug << ", " << reply->errorString();
        }
    }
    if (job->_timedout) {
        debug << ", timedout";
    }
    debug << ")";
    return debug.maybeSpace();
}

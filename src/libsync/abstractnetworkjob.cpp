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

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {
constexpr int MaxRetryCount = 5;
}


namespace OCC {

Q_LOGGING_CATEGORY(lcNetworkJob, "sync.networkjob", QtInfoMsg)

// If not set, it is overwritten by the Application constructor with the value from the config
seconds AbstractNetworkJob::httpTimeout = [] {
    const auto def = qEnvironmentVariableIntValue("OWNCLOUD_TIMEOUT");
    if (def <= 0) {
        return AbstractNetworkJob::DefaultHttpTimeout;
    }
    return seconds(def);
}();

AbstractNetworkJob::AbstractNetworkJob(AccountPtr account, const QUrl &baseUrl, const QString &path, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _baseUrl(baseUrl)
    , _path(path)
{
    // Since we hold a QSharedPointer to the account, this makes no sense. (issue #6893)
    Q_ASSERT(account != parent);
    Q_ASSERT(baseUrl.isValid());
}

QUrl AbstractNetworkJob::baseUrl() const
{
    return _baseUrl;
}

QUrl AbstractNetworkJob::url() const
{
    return Utility::concatUrlPath(baseUrl(), path(), query());
}


void AbstractNetworkJob::setQuery(const QUrlQuery &query)
{
    _query = query;
}

QUrlQuery AbstractNetworkJob::query() const
{
    return _query;
}

void AbstractNetworkJob::setTimeout(const std::chrono::seconds sec)
{
    _timeout = sec;
}

void AbstractNetworkJob::setIgnoreCredentialFailure(bool ignore)
{
    _ignoreCredentialFailure = ignore;
}

QNetworkReply *AbstractNetworkJob::reply() const
{
    Q_ASSERT(_reply);
    return _reply;
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
        if (_reply->error() == QNetworkReply::ContentReSendError && _reply->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool()) {
            return true;
        }
    }
    return false;
}

void AbstractNetworkJob::sendRequest(const QByteArray &verb,
    const QNetworkRequest &req, QIODevice *requestBody)
{
    _verb = verb;
    _request = req;
    _requestBody = requestBody;
    Q_ASSERT(_request.url().isEmpty() || _request.url() == url());
    Q_ASSERT(_request.transferTimeout() == 0 || _request.transferTimeout() == duration_cast<milliseconds>(_timeout).count());
    _request.setUrl(url());
    _request.setPriority(_priority);
    _request.setTransferTimeout(duration_cast<milliseconds>(_timeout).count());
    if (!isAuthenticationJob() && _account->jobQueue()->enqueue(this)) {
        return;
    }
    auto reply = _account->sendRawRequest(verb, _request.url(), _request, requestBody);
    if (_requestBody) {
        _requestBody->setParent(this);
    }
    adoptRequest(reply);
}

void AbstractNetworkJob::adoptRequest(QPointer<QNetworkReply> reply)
{
    std::swap(_reply, reply);
    delete reply;

    _request = _reply->request();

    connect(_reply, &QNetworkReply::finished, this, &AbstractNetworkJob::slotFinished);

    newReplyHook(_reply);
}

void AbstractNetworkJob::slotFinished()
{
    _finished = true;
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

        if (_reply->error() == QNetworkReply::OperationCanceledError && !_aborted) {
            _timedout = true;
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
    Q_EMIT aboutToFinishSignal(AbstractNetworkJob::QPrivateSignal());
    finished();
    Q_EMIT finishedSignal(AbstractNetworkJob::QPrivateSignal());
    qCDebug(lcNetworkJob) << "Network job finished" << this;
    deleteLater();
}

QByteArray AbstractNetworkJob::responseTimestamp() const
{
    OC_ASSERT(!_responseTimestamp.isEmpty() || _aborted || (reply() && reply()->error() == QNetworkReply::RemoteHostClosedError));
    return _responseTimestamp;
}

QDateTime OCC::AbstractNetworkJob::responseQTimeStamp() const
{
    return QDateTime::fromString(QString::fromUtf8(responseTimestamp()), Qt::RFC2822Date);
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
    if (!_finished && !_aborted && !_timedout) {
        qCCritical(lcNetworkJob) << "Deleting running job" << this << parent();
    }
    delete _reply;
    _reply = nullptr;
}

void AbstractNetworkJob::start()
{
    qCInfo(lcNetworkJob) << "Created" << this << "for" << parent();
}

QString AbstractNetworkJob::replyStatusString() {
    Q_ASSERT(reply());
    if (reply()->error() == QNetworkReply::NoError) {
        return QStringLiteral("OK");
    } else {
        return QStringLiteral("%1, %2").arg(Utility::enumToString(reply()->error()), errorString());
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
    qCInfo(lcNetworkJob) << "Restarting" << this << "for the" << _retryCount << "time";
    if (_requestBody) {
        if (_requestBody->isSequential()) {
            Q_ASSERT(_requestBody->isOpen());
            _requestBody->seek(0);
        } else {
            qCWarning(lcNetworkJob) << "Can't resend request, body not suitable" << this;
            abort();
            return;
        }
    }
    sendRequest(_verb, _request, _requestBody);
}

void AbstractNetworkJob::abort()
{
    if (_reply) {
        // calling abort will trigger the execution of finished()
        // with _reply->error() == QNetworkReply::OperationCanceledError
        // the api user can then decide whether to discard this job or retry it.
        // The order is important, mark as _aborted before we abort the reply which will trigger slotFinished
        _aborted = true;
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

int AbstractNetworkJob::httpStatusCode() const
{
    return reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

} // namespace OCC

QDebug operator<<(QDebug debug, const OCC::AbstractNetworkJob *job)
{
    QDebugStateSaver saver(debug);
    debug.setAutoInsertSpaces(false);
    debug << job->metaObject()->className() << "(" << job->account().data() << ", " << job->url().toDisplayString()
          << ", " << job->_verb;
    if (auto reply = job->_reply) {
        debug << ", Original-Request-ID: " << reply->request().rawHeader("Original-Request-ID")
              << ", X-Request-ID: " << reply->request().rawHeader("X-Request-ID");

        const auto errorString = reply->rawHeader(QByteArrayLiteral("OC-ErrorString"));
        if (!errorString.isEmpty()) {
            debug << ", Error:" << errorString;
        }
        if (reply->error() != QNetworkReply::NoError) {
            debug << ", NetworkError: " << reply->errorString();
        }
    }
    if (job->_timedout) {
        debug << ", timedout";
    }
    debug << ")";
    return debug.maybeSpace();
}

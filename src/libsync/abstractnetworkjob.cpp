/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QLoggingCategory>
#include <QHstsPolicy>
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
bool AbstractNetworkJob::enableTimeout = false;

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
    // Qt doesn't yet transparently resend HTTP2 requests, do so here
    const auto maxHttp2Resends = 3;
    QByteArray verb = HttpLogger::requestVerb(*reply());
    if (_reply->error() == QNetworkReply::ContentReSendError
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

        if (_account->credentials()->retryIfNeeded(this))
            return;

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
        // Redirects may be relative
        if (redirectUrl.isRelative())
            redirectUrl = requestedUrl.resolved(redirectUrl);

        // For POST requests where the target url has query arguments, Qt automatically
        // moves these arguments to the body if no explicit body is specified.
        // This can cause problems with redirected requests, because the redirect url
        // will no longer contain these query arguments.
        if (reply()->operation() == QNetworkAccessManager::PostOperation
            && requestedUrl.hasQuery()
            && !redirectUrl.hasQuery()
            && !_requestBody) {
            qCWarning(lcNetworkJob) << "Redirecting a POST request with an implicit body loses that body";
        }

        // ### some of the qWarnings here should be exported via displayErrors() so they
        // ### can be presented to the user if the job executor has a GUI
        if (requestedUrl.scheme() == QLatin1String("https") && redirectUrl.scheme() == QLatin1String("http")) {
            qCWarning(lcNetworkJob) << this << "HTTPS->HTTP downgrade detected!";
        } else if (requestedUrl == redirectUrl || _redirectCount + 1 >= maxRedirects()) {
            qCWarning(lcNetworkJob) << this << "Redirect loop detected!";
        } else if (_requestBody && _requestBody->isSequential()) {
            qCWarning(lcNetworkJob) << this << "cannot redirect request with sequential body";
        } else if (verb.isEmpty()) {
            qCWarning(lcNetworkJob) << this << "cannot redirect request: could not detect original verb";
        } else {
            emit redirected(_reply, redirectUrl, _redirectCount);

            // The signal emission may have changed this value
            if (_followRedirects) {
                _redirectCount++;

                // Create the redirected request and send it
                qCInfo(lcNetworkJob) << "Redirecting" << verb << requestedUrl << redirectUrl;
                resetTimeout();
                if (_requestBody) {
                    if(!_requestBody->isOpen()) {
                        // Avoid the QIODevice::seek (QBuffer): The device is not open warning message
                       _requestBody->open(QIODevice::ReadOnly);
                    }
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

QByteArray AbstractNetworkJob::requestId()
{
    return  _reply ? _reply->request().rawHeader("X-Request-ID") : QByteArray();
}

QString AbstractNetworkJob::errorString() const
{
    if (_timedout) {
        return tr("The server took too long to respond. Check your connection and try syncing again. If it still doesn’t work, reach out to your server administrator.");
    }

    if (!reply()) {
        return tr("An unexpected error occurred. Please try syncing again or contact your server administrator if the issue continues.");
    }

    if (reply()->hasRawHeader("OC-ErrorString")) {
        return reply()->rawHeader("OC-ErrorString");
    }

    if (const auto hstsError = hstsErrorStringFromReply(reply())) {
        return *hstsError;
    }

    return networkReplyErrorString(*reply());
}

QString AbstractNetworkJob::errorStringParsingBody(QByteArray *body)
{
    const auto errorMessage = errorString();
    if (errorMessage.isEmpty() || !reply()) {
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

    return errorMessage;
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
    const QString displayUrl = QStringLiteral("%1://%2%3").arg(url.scheme()).arg(url.host()).arg(url.path());

    QString parentMetaObjectName = parent() ? parent()->metaObject()->className() : "";
    qCInfo(lcNetworkJob) << metaObject()->className() << "created for" << displayUrl << "+" << path() << parentMetaObjectName;
}

void AbstractNetworkJob::slotTimeout()
{
    // TODO: workaround, find cause of https://github.com/nextcloud/desktop/issues/7184
    if (!AbstractNetworkJob::enableTimeout) {
        return;
    }

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
    if (reader.name() != QStringLiteral("error")) {
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

    qCWarning(lcNetworkJob) << "Network request error" << base << "HTTP status" << httpStatus << "httpReason" << httpReason;

    QString userFriendlyMessage;
    switch (httpStatus) {
        case 400: //Bad Request
            userFriendlyMessage = QObject::tr("We couldn’t process your request. Please try syncing again later. If this keeps happening, contact your server administrator for help.");
            break;
        case 401: //Unauthorized
            userFriendlyMessage = QObject::tr("You need to sign in to continue. If you have trouble with your credentials, please reach out to your server administrator.");
            break;
        case 403: //Forbidden
            userFriendlyMessage = QObject::tr("You don’t have access to this resource. If you think this is a mistake, please contact your server administrator.");
            break;
        case 404: //Not Found
            userFriendlyMessage = QObject::tr("We couldn’t find what you were looking for. It might have been moved or deleted. If you need help, contact your server administrator.");
            break;
        case 407: //Proxy Authentication Required
            userFriendlyMessage = QObject::tr("It seems you are using a proxy that required authentication. Please check your proxy settings and credentials. If you need help, contact your server administrator.");
            break;
        case 408: //Request Timeout
            userFriendlyMessage = QObject::tr("The request is taking longer than usual. Please try syncing again. If it still doesn’t work, reach out to your server administrator.");
            break;
        case 409: //Conflict
            userFriendlyMessage = QObject::tr("Server files changed while you were working. Please try syncing again. Contact your server administrator if the issue persists.");
            break;
        case 410: //Gone
            userFriendlyMessage = QObject::tr("This folder or file isn’t available anymore. If you need assistance, please contact your server administrator.");
            break;
        case 412: //Precondition failed
            userFriendlyMessage = QObject::tr("The request could not be completed because some required conditions were not met. Please try syncing again later. If you need assistance, please contact your server administrator.");
            break;
        case 413: //Payload Too Large
            userFriendlyMessage = QObject::tr("The file is too big to upload. You might need to choose a smaller file or contact your server administrator for assistance.");
            break;
        case 414: //URI Too Long
            userFriendlyMessage = QObject::tr("The address used to make the request is too long for the server to handle. Please try shortening the information you’re sending or contact your server administrator for assistance.");
            break;
        case 415: //Unsupported Media Type
            userFriendlyMessage = QObject::tr("This file type isn’t supported. Please contact your server administrator for assistance.");
            break;
        case 422: //Unprocessable Entity
            userFriendlyMessage = QObject::tr("The server couldn’t process your request because some information was incorrect or incomplete. Please try syncing again later, or contact your server administrator for assistance.");
            break;
        case 423: //Locked
            userFriendlyMessage = QObject::tr("The resource you are trying to access is currently locked and cannot be modified. Please try changing it later, or contact your server administrator for assistance.");
            break;
        case 428: //Precondition Required
            userFriendlyMessage = QObject::tr("This request could not be completed because it is missing some required conditions. Please try again later, or contact your server administrator for help.");
            break;
        case 429: //Too Many Requests
            userFriendlyMessage = QObject::tr("You made too many requests. Please wait and try again. If you keep seeing this, your server administrator can help.");
            break;
        case 500: //Internal Server Error
            userFriendlyMessage = QObject::tr("Something went wrong on the server. Please try syncing again later, or contact your server administrator if the issue persists.");
            break;
        case 501: //Not Implemented
            userFriendlyMessage = QObject::tr("The server does not recognize the request method. Please contact your server administrator for help.");
            break;
        case 502: //Bad Gateway
            userFriendlyMessage = QObject::tr("We’re having trouble connecting to the server. Please try again soon. If the issue persists, your server administrator can help you.");
            break;
        case 503: //Service Unavailable
            userFriendlyMessage = QObject::tr("The server is busy right now. Please try syncing again in a few minutes or contact your server administrator if it’s urgent.");
            break;
        case 504: //Gateway Timeout
            userFriendlyMessage = QObject::tr("It’s taking too long to connect to the server. Please try again later. If you need help, contact your server administrator.");
            break;
        case 505: //HTTP Version Not Supported
            userFriendlyMessage = QObject::tr("The server does not support the version of the connection being used. Contact your server administrator for help.");
            break;
        case 507: //Insufficient Storage
            userFriendlyMessage = QObject::tr("The server does not have enough space to complete your request. Please check how much quota your user has by contacting your server administrator.");
            break;
        case 511: //Network Authentication Required
            userFriendlyMessage = QObject::tr("Your network needs extra authentication. Please check your connection. Contact your server administrator for help if the issue persists.");
            break;
        case 513: //Resource Not Authorized
            userFriendlyMessage = QObject::tr("You don’t have permission to access this resource. If you believe this is an error, contact your server administrator to ask for assistance.");
            break;
        default:
            userFriendlyMessage = QObject::tr("An unexpected error occurred. Please try syncing again or contact contact your server administrator if the issue continues.");
            break;
    }

    return userFriendlyMessage;
}

void AbstractNetworkJob::retry()
{
    Q_ASSERT(_reply);
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

std::optional<QString> AbstractNetworkJob::hstsErrorStringFromReply(QNetworkReply *reply)
{
    if (!reply) {
        return {};
    }

    if (reply->error() != QNetworkReply::SslHandshakeFailedError) {
        return {};
    }

    if (!(reply->manager() && reply->manager()->isStrictTransportSecurityEnabled())) {
        return {};
    }

    const auto host = reply->url().host();
    const auto policies = reply->manager()->strictTransportSecurityHosts();
    for (const auto &policy : policies) {
        if (policy.host() == host && !policy.isExpired()) {
            return tr("The server enforces strict transport security and does not accept untrusted certificates.");
        }
    }

    return {};
}

} // namespace OCC

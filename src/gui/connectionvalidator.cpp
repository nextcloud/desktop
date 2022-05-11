/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkProxyFactory>
#include <QXmlStreamReader>

#include "account.h"
#include "clientproxy.h"
#include "connectionvalidator.h"
#include "creds/abstractcredentials.h"
#include "networkjobs.h"
#include "networkjobs/jsonjob.h"
#include "theme.h"
#include "tlserrordialog.h"

using namespace std::chrono_literals;

namespace OCC {

Q_LOGGING_CATEGORY(lcConnectionValidator, "sync.connectionvalidator", QtInfoMsg)

// Make sure the timeout for this job is less than how often we get called
// This makes sure we get tried often enough without "ConnectionValidator already running"
namespace {
    const auto timeoutToUse = ConnectionValidator::DefaultCallingInterval - 5s;
}

ConnectionValidator::ConnectionValidator(AccountPtr account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
}

void ConnectionValidator::setClearCookies(bool clearCookies)
{
    _clearCookies = clearCookies;
}

void ConnectionValidator::checkServer()
{
    _updateConfig = false;
    checkServerAndUpdate();
}

void ConnectionValidator::checkServerAndUpdate()
{
    if (!_account) {
        _errors << tr("No ownCloud account configured");
        reportResult(NotConfigured);
        return;
    }
    qCDebug(lcConnectionValidator) << "Checking server and authentication";

    // Lookup system proxy in a thread https://github.com/owncloud/client/issues/2993
    if (ClientProxy::isUsingSystemDefault()) {
        qCDebug(lcConnectionValidator) << "Trying to look up system proxy";
        ClientProxy::lookupSystemProxyAsync(_account->url(),
            this, SLOT(systemProxyLookupDone(QNetworkProxy)));
    } else {
        // We want to reset the QNAM proxy so that the global proxy settings are used (via ClientProxy settings)
        _account->accessManager()->setProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
        // use a queued invocation so we're as asynchronous as with the other code path
        QMetaObject::invokeMethod(this, "slotCheckServerAndAuth", Qt::QueuedConnection);
    }
}

void ConnectionValidator::systemProxyLookupDone(const QNetworkProxy &proxy)
{
    if (!_account) {
        qCWarning(lcConnectionValidator) << "Bailing out, Account had been deleted";
        return;
    }

    if (proxy.type() != QNetworkProxy::NoProxy) {
        qCInfo(lcConnectionValidator) << "Setting QNAM proxy to be system proxy" << printQNetworkProxy(proxy);
    } else {
        qCInfo(lcConnectionValidator) << "No system proxy set by OS";
    }
    _account->accessManager()->setProxy(proxy);

    slotCheckServerAndAuth();
}

// The actual check
void ConnectionValidator::slotCheckServerAndAuth()
{
    // ensure we receive ssl errors
    _account->resetAccessManager();
    CheckServerJob *checkJob = new CheckServerJob(_account, this);
    checkJob->setClearCookies(_clearCookies);
    checkJob->setTimeout(timeoutToUse);
    connect(checkJob, &CheckServerJob::instanceFound, this, &ConnectionValidator::slotStatusFound);
    connect(checkJob, &CheckServerJob::instanceNotFound, this, &ConnectionValidator::slotNoStatusFound);
    connect(checkJob, &CheckServerJob::timeout, this, [checkJob, this] {
        qCWarning(lcConnectionValidator) << checkJob;
        _errors.append(tr("timeout"));
        reportResult(Timeout);
    });
    connect(checkJob, &CheckServerJob::sslErrors, this, &ConnectionValidator::sslErrors);
    checkJob->start();
}

void ConnectionValidator::slotStatusFound(const QUrl &url, const QJsonObject &info)
{
    // Newer servers don't disclose any version in status.php anymore
    // https://github.com/owncloud/core/pull/27473/files
    // so this string can be empty.
    QString serverVersion = CheckServerJob::version(info);

    // status.php was found.
    qCInfo(lcConnectionValidator) << "** Application: ownCloud found: "
                                  << url << " with version "
                                  << CheckServerJob::versionString(info)
                                  << "(" << serverVersion << ")";

    // Update server url in case of redirection
    if (_account->url() != url) {
        qCInfo(lcConnectionValidator()) << "status.php was redirected to" << url.toString() << "asking user to accept and abort for now";
        Q_EMIT _account->requestUrlUpdate(url);
        reportResult(StatusNotFound);
        return;
    }

    if (!serverVersion.isEmpty() && !setAndCheckServerVersion(serverVersion)) {
        return;
    }

    // Check for maintenance mode: Servers send "true", so go through QVariant
    // to parse it correctly.
    if (info["maintenance"].toVariant().toBool()) {
        reportResult(MaintenanceMode);
        return;
    }

    // now check the authentication
    QTimer::singleShot(0, this, &ConnectionValidator::checkAuthentication);
}

// status.php could not be loaded (network or server issue!).
void ConnectionValidator::slotNoStatusFound(QNetworkReply *reply)
{
    auto job = qobject_cast<CheckServerJob *>(sender());
    qCWarning(lcConnectionValidator) << reply->error() << job << reply->peek(1024);
    if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
        reportResult(SslError);
        return;
    } else if (reply->error() == QNetworkReply::TooManyRedirectsError) {
        reportResult(MaintenanceMode);
        return;
    } else if (!_account->credentials()->stillValid(reply)) {
        // Note: Why would this happen on a status.php request?
        _errors.append(tr("Authentication error: Either username or password are wrong."));
    } else {
        //_errors.append(tr("Unable to connect to %1").arg(_account->url().toString()));
        _errors.append(job->errorString());
    }
    reportResult(StatusNotFound);
}

void ConnectionValidator::checkAuthentication()
{
    AbstractCredentials *creds = _account->credentials();

    if (!creds->ready()) {
        reportResult(CredentialsNotReady);
        return;
    }

    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
    qCDebug(lcConnectionValidator) << "# Check whether authenticated propfind works.";

    // we explicitly use a legacy dav path here
    PropfindJob *job = new PropfindJob(_account, _account->url(), Theme::instance()->webDavPath(), this);
    job->setAuthenticationJob(true); // don't retry
    job->setTimeout(timeoutToUse);
    job->setProperties({ QByteArrayLiteral("getlastmodified") });
    connect(job, &PropfindJob::finishedWithoutError, this, &ConnectionValidator::slotAuthSuccess);
    connect(job, &PropfindJob::finishedWithError, this, &ConnectionValidator::slotAuthFailed);
    job->start();
}

void ConnectionValidator::slotAuthFailed(QNetworkReply *reply)
{
    auto job = qobject_cast<PropfindJob *>(sender());
    Status stat = Timeout;

    if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
        _errors << job->errorStringParsingBody();
        stat = SslError;

    } else if (reply->error() == QNetworkReply::AuthenticationRequiredError
        || !_account->credentials()->stillValid(reply)) {
        qCWarning(lcConnectionValidator) << "******** Password is wrong!" << reply->error() << job;
        _errors << tr("The provided credentials are not correct");
        stat = CredentialsWrong;

    } else if (reply->error() != QNetworkReply::NoError) {
        _errors << job->errorStringParsingBody();

        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 503) {
            _errors.clear();
            stat = ServiceUnavailable;
        }
    }

    reportResult(stat);
}

void ConnectionValidator::slotAuthSuccess()
{
    _errors.clear();
    if (!_updateConfig) {
        reportResult(Connected);
        return;
    }

    checkServerCapabilities();
}

void ConnectionValidator::checkServerCapabilities()
{
    // The main flow now needs the capabilities
    auto *job = new JsonApiJob(_account, QStringLiteral("ocs/v2.php/cloud/capabilities"), {}, {}, this);
    job->setAuthenticationJob(true);
    job->setTimeout(timeoutToUse);

    QObject::connect(job, &JsonApiJob::finishedSignal, this, [job, this] {
        auto caps = job->data().value("ocs").toObject().value("data").toObject().value("capabilities").toObject();
        qCInfo(lcConnectionValidator) << "Server capabilities" << caps;
        _account->setCapabilities(caps.toVariantMap());

        // New servers also report the version in the capabilities
        QString serverVersion = caps["core"].toObject()["status"].toObject()["version"].toString();
        if (!serverVersion.isEmpty() && !setAndCheckServerVersion(serverVersion)) {
            return;
        }

        fetchUser();
    });
    job->start();
}

void ConnectionValidator::fetchUser()
{
    auto *job = new JsonApiJob(_account, QLatin1String("ocs/v2.php/cloud/user"), {}, {}, this);
    job->setTimeout(timeoutToUse);
    job->setAuthenticationJob(true);
    QObject::connect(job, &JsonApiJob::finishedSignal, this, [job, this] {
        const QString user = job->data().value("ocs").toObject().value("data").toObject().value("id").toString();
        if (!user.isEmpty()) {
            _account->setDavUser(user);
        }
        const QString displayName = job->data().value("ocs").toObject().value("data").toObject().value("display-name").toString();
        if (!displayName.isEmpty()) {
            _account->setDavDisplayName(displayName);
        }

        auto capabilities = _account->capabilities();
        // We should have received the capabilities by now. Check that assumption in a debug build. If
        // it's not the case, the code below will assume that they are not available.
        Q_ASSERT(capabilities.isValid());

#ifndef TOKEN_AUTH_ONLY
        if (capabilities.isValid() && capabilities.avatarsAvailable()) {
            auto *job = new AvatarJob(_account, _account->davUser(), 128, this);
            job->setAuthenticationJob(true);
            job->setTimeout(20s);
            QObject::connect(job, &AvatarJob::avatarPixmap, this, &ConnectionValidator::slotAvatarImage);
            job->start();
            // reportResult will be called when the avatar has been received by `slotAvatarImage`
        } else
#endif
        {
            reportResult(Connected);
        }
    });
    job->start();
}

bool ConnectionValidator::setAndCheckServerVersion(const QString &version)
{
    _account->setServerVersion(version);

    // We cannot deal with servers < 7.0.0
    if (_account->serverVersionInt()
        && _account->serverVersionInt() < Account::makeServerVersion(7, 0, 0)) {
        _errors.append(tr("The configured server for this client is too old"));
        _errors.append(tr("Please update to the latest server and restart the client."));
        reportResult(ServerVersionMismatch);
        return false;
    }
    // We attempt to work with servers >= 7.0.0 but warn users.
    // Check usages of Account::serverVersionUnsupported() for details.

    // Record that the server supports HTTP/2
    // Actual decision if we should use HTTP/2 is done in AccessManager::createRequest
    if (auto job = qobject_cast<AbstractNetworkJob *>(sender())) {
        if (auto reply = job->reply()) {
            _account->setHttp2Supported(
                reply->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool());
        }
    }
    return true;
}

#ifndef TOKEN_AUTH_ONLY
void ConnectionValidator::slotAvatarImage(const QPixmap &img)
{
    _account->setAvatar(img);
    reportResult(Connected);
}
#endif

void ConnectionValidator::reportResult(Status status)
{
    emit connectionResult(status, _errors);
    deleteLater();
}

} // namespace OCC

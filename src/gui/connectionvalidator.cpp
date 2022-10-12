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
#include "cookiejar.h"
#include "creds/abstractcredentials.h"
#include "networkjobs.h"
#include "networkjobs/checkserverjobfactory.h"
#include "networkjobs/jobgroup.h"
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

void ConnectionValidator::checkServer(ConnectionValidator::ValidationMode mode)
{
    _mode = mode;
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
        QMetaObject::invokeMethod(this, &ConnectionValidator::slotCheckServerAndAuth, Qt::QueuedConnection);
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
    // in order to receive all ssl erorrs we need a fresh QNam
    auto nam = _account->credentials()->createAM();
    nam->setCustomTrustedCaCertificates(_account->approvedCerts());

    // do we start with the old cookies or new
    if (!_clearCookies) {
        const auto accountCookies = _account->accessManager()->ownCloudCookieJar()->allCookies();
        nam->ownCloudCookieJar()->setAllCookies(accountCookies);
    }

    auto checkServerJob = CheckServerJobFactory(nam).startJob(_account->url());

    connect(nam, &AccessManager::sslErrors, this, [this](QNetworkReply *reply, const QList<QSslError> &errors) {
        Q_UNUSED(reply)
        Q_EMIT sslErrors(errors);
    });

    connect(checkServerJob, &CoreJob::finished, this, [checkServerJob, nam, this]() {
        nam->deleteLater();
        if (checkServerJob->success()) {
            const auto result = checkServerJob->result().value<CheckServerJobResult>();

            // adopt the new cookies
            _account->accessManager()->setCookieJar(nam->cookieJar());

            slotStatusFound(result.serverUrl(), result.statusObject());
        } else {
            switch (checkServerJob->reply()->error()) {
            case QNetworkReply::OperationCanceledError:
                qCWarning(lcConnectionValidator) << checkServerJob;
                _errors.append(tr("timeout"));
                reportResult(Timeout);
                return;
            case QNetworkReply::SslHandshakeFailedError:
                reportResult(SslError);
                return;
            case QNetworkReply::TooManyRedirectsError:
                reportResult(MaintenanceMode);
                return;
            default:
                break;
            }

            if (!_account->credentials()->stillValid(checkServerJob->reply())) {
                // Note: Why would this happen on a status.php request?
                _errors.append(tr("Authentication error: Either username or password are wrong."));
            } else {
                //_errors.append(tr("Unable to connect to %1").arg(_account->url().toString()));
                _errors.append(checkServerJob->errorMessage());
            }
            reportResult(StatusNotFound);
        }
    });
}

void ConnectionValidator::slotStatusFound(const QUrl &url, const QJsonObject &info)
{
    // status.php was found.
    qCInfo(lcConnectionValidator) << "** Application: ownCloud found: "
                                  << url << " with version "
                                  << info.value(QLatin1String("versionstring")).toString();

    // Update server url in case of redirection
    if (_account->url() != url) {
        qCInfo(lcConnectionValidator()) << "status.php was redirected to" << url.toString() << "asking user to accept and abort for now";
        Q_EMIT _account->requestUrlUpdate(url);
        reportResult(StatusNotFound);
        return;
    }

    // Check for maintenance mode: Servers send "true", so go through QVariant
    // to parse it correctly.
    if (info[QStringLiteral("maintenance")].toVariant().toBool()) {
        reportResult(MaintenanceMode);
        return;
    }

    AbstractCredentials *creds = _account->credentials();
    if (!creds->ready()) {
        reportResult(CredentialsNotReady);
        return;
    }
    // now check the authentication
    if (_mode != ConnectionValidator::ValidationMode::ValidateServer) {
        checkAuthentication();
    } else {
        reportResult(Connected);
    }
}

void ConnectionValidator::checkAuthentication()
{
    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
    qCDebug(lcConnectionValidator) << "# Check whether authenticated propfind works.";

    // we explicitly use a legacy dav path here
    auto *job = new PropfindJob(_account, _account->url(), Theme::instance()->webDavPath(), PropfindJob::Depth::Zero, this);
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
    if (_mode != ConnectionValidator::ValidationMode::ValidateAuth) {
        checkServerCapabilities();
        return;
    }
    reportResult(Connected);
}

void ConnectionValidator::checkServerCapabilities()
{
    // The main flow now needs the capabilities
    auto *job = new JsonApiJob(_account, QStringLiteral("ocs/v2.php/cloud/capabilities"), {}, {}, this);
    job->setAuthenticationJob(true);
    job->setTimeout(timeoutToUse);

    QObject::connect(job, &JsonApiJob::finishedSignal, this, [job, this] {
        auto caps = job->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("capabilities")).toObject();
        qCInfo(lcConnectionValidator) << "Server capabilities" << caps;
        _account->setCapabilities(caps.toVariantMap());
        if (!checkServerInfo()) {
            return;
        }

        fetchUser();
    });
    job->start();
}

void ConnectionValidator::fetchUser()
{
    auto *job = new JsonApiJob(_account, QStringLiteral("ocs/v2.php/cloud/user"), {}, {}, this);
    job->setTimeout(timeoutToUse);
    job->setAuthenticationJob(true);
    QObject::connect(job, &JsonApiJob::finishedSignal, this, [job, this] {
        const QString user = job->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("id")).toString();
        if (!user.isEmpty()) {
            _account->setDavUser(user);
        }
        const QString displayName = job->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("display-name")).toString();
        if (!displayName.isEmpty()) {
            _account->setDavDisplayName(displayName);
        }

        auto capabilities = _account->capabilities();
        // We should have received the capabilities by now. Check that assumption in a debug build. If
        // it's not the case, the code below will assume that they are not available.
        Q_ASSERT(capabilities.isValid());

        auto *group = new JobGroup(this);
        if (capabilities.isValid()) {
            if (capabilities.avatarsAvailable()) {
                auto *avatarJob = group->createJob<AvatarJob>(_account, _account->davUser(), 128, this);
                avatarJob->setAuthenticationJob(true);
                avatarJob->setTimeout(20s);
                connect(avatarJob, &AvatarJob::avatarPixmap, this, [this](const QPixmap &img) {
                    _account->setAvatar(img);
                });
                avatarJob->start();
            }
            if (capabilities.appProviders().enabled) {
                auto *jsonJob = group->createJob<JsonJob>(_account, _account->url(), capabilities.appProviders().appsUrl, "GET");
                connect(jsonJob, &JsonApiJob::finishedSignal, this, [jsonJob, this] {
                    _account->setAppProvider(AppProvider { jsonJob->data() });
                });
                jsonJob->start();
            }
        }
        connect(group, &JobGroup::finishedSignal, this, [group, this] {
            group->deleteLater();
            reportResult(Connected);
        });

        if (group->isEmpty()) {
            Q_EMIT group->finishedSignal();
        }
    });
    job->start();
}

bool ConnectionValidator::checkServerInfo()
{
    // We cannot deal with servers < 10.0.0
    if (_account->serverVersionUnsupported()) {
        _errors.append(tr("The configured server for this client is too old."));
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

void ConnectionValidator::reportResult(Status status)
{
    emit connectionResult(status, _errors);
    deleteLater();
}

} // namespace OCC

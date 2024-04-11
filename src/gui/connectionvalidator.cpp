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
#include "gui/connectionvalidator.h"
#include "gui/clientproxy.h"
#include "gui/fetchserversettings.h"
#include "gui/networkinformation.h"
#include "gui/tlserrordialog.h"
#include "libsync/account.h"
#include "libsync/cookiejar.h"
#include "libsync/creds/abstractcredentials.h"
#include "libsync/networkjobs.h"
#include "libsync/networkjobs/checkserverjobfactory.h"
#include "libsync/networkjobs/jsonjob.h"
#include "libsync/theme.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkProxyFactory>
#include <QXmlStreamReader>


using namespace std::chrono_literals;

namespace OCC {

Q_LOGGING_CATEGORY(lcConnectionValidator, "sync.connectionvalidator", QtInfoMsg)

// Make sure the timeout for this job is less than how often we get called
// This makes sure we get tried often enough without "ConnectionValidator already running"
namespace {
    auto timeoutToUse()
    {
        return std::min(ConnectionValidator::DefaultCallingInterval - 5s, AbstractNetworkJob::httpTimeout);
    };
}

ConnectionValidator::ConnectionValidator(AccountPtr account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
    // TODO: 6.0 abort validator on 5min timeout
    auto timer = new QTimer(this);
    timer->setInterval(30s);
    connect(timer, &QTimer::timeout, this,
        [this] { qCInfo(lcConnectionValidator) << "ConnectionValidator" << _account->displayName() << "still running after" << _duration.duration(); });
    timer->start();
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
        qCInfo(lcConnectionValidator) << "Setting QNAM proxy to be system proxy" << ClientProxy::printQNetworkProxy(proxy);
    } else {
        qCInfo(lcConnectionValidator) << "No system proxy set by OS";
    }
    _account->accessManager()->setProxy(proxy);

    slotCheckServerAndAuth();
}

// The actual check
void ConnectionValidator::slotCheckServerAndAuth()
{
    auto checkServerFactory = CheckServerJobFactory::createFromAccount(_account, _clearCookies, this);
    auto checkServerJob = checkServerFactory.startJob(_account->url(), this);

    connect(checkServerJob->reply()->manager(), &AccessManager::sslErrors, this, [this](QNetworkReply *reply, const QList<QSslError> &errors) {
        Q_UNUSED(reply)
        Q_EMIT sslErrors(errors);
    });

    connect(checkServerJob, &CoreJob::finished, this, [checkServerJob, this]() {
        if (checkServerJob->success()) {
            const auto result = checkServerJob->result().value<CheckServerJobResult>();

            // adopt the new cookies
            _account->accessManager()->setCookieJar(checkServerJob->reply()->manager()->cookieJar());

            slotStatusFound(result.serverUrl(), result.statusObject());
        } else {
            switch (checkServerJob->reply()->error()) {
            case QNetworkReply::OperationCanceledError:
                [[fallthrough]];
            case QNetworkReply::TimeoutError:
                qCWarning(lcConnectionValidator) << checkServerJob;
                _errors.append(tr("timeout"));
                reportResult(Timeout);
                return;
            case QNetworkReply::SslHandshakeFailedError:
                reportResult(NetworkInformation::instance()->isBehindCaptivePortal() ? CaptivePortal : SslError);
                return;
            case QNetworkReply::TooManyRedirectsError:
                reportResult(MaintenanceMode);
                return;
            case QNetworkReply::ContentAccessDenied:
                reportResult(ClientUnsupported);
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
    job->setTimeout(timeoutToUse());
    job->setProperties({ QByteArrayLiteral("getlastmodified") });
    connect(job, &PropfindJob::finishedWithoutError, this, &ConnectionValidator::slotAuthSuccess);
    connect(job, &PropfindJob::finishedWithError, this, &ConnectionValidator::slotAuthFailed);
    job->start();
}

void ConnectionValidator::slotAuthFailed()
{
    auto job = qobject_cast<PropfindJob *>(sender());
    Status stat = Timeout;

    if (job->reply()->error() == QNetworkReply::SslHandshakeFailedError) {
        _errors << job->errorStringParsingBody();
        stat = NetworkInformation::instance()->isBehindCaptivePortal() ? CaptivePortal : SslError;

    } else if (job->reply()->error() == QNetworkReply::AuthenticationRequiredError || !_account->credentials()->stillValid(job->reply())) {
        qCWarning(lcConnectionValidator) << "******** Password is wrong!" << job->reply()->error() << job;
        _errors << tr("The provided credentials are not correct");
        stat = CredentialsWrong;
    } else if (job->reply()->error() == QNetworkReply::ContentAccessDenied) {
        stat = ClientUnsupported;
        _errors << extractErrorMessage(job->reply()->readAll());
    } else if (job->reply()->error() != QNetworkReply::NoError) {
        _errors << job->errorStringParsingBody();

        if (job->httpStatusCode() == 503) {
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
        auto *fetchSetting = new FetchServerSettingsJob(_account, this);
        const auto unsupportedServerError = [this] {
            _errors.append({tr("The configured server for this client is too old."), tr("Please update to the latest server and restart the client.")});
        };
        connect(fetchSetting, &FetchServerSettingsJob::finishedSignal, this, [unsupportedServerError, this](FetchServerSettingsJob::Result result) {
            switch (result) {
            case FetchServerSettingsJob::Result::UnsupportedServer:
                unsupportedServerError();
                reportResult(ServerVersionMismatch);
                break;
            case FetchServerSettingsJob::Result::InvalidCredentials:
                reportResult(CredentialsWrong);
                break;
            case FetchServerSettingsJob::Result::TimeOut:
                reportResult(Timeout);
                break;
            case FetchServerSettingsJob::Result::Success:
                if (_account->serverSupportLevel() == Account::ServerSupportLevel::Unknown) {
                    unsupportedServerError();
                }
                reportResult(Connected);
                break;
            case FetchServerSettingsJob::Result::Undefined:
                reportResult(Undefined);
                break;
            }
        });

        fetchSetting->start();
        return;
    }
    reportResult(Connected);
}

void ConnectionValidator::reportResult(Status status)
{
    if (OC_ENSURE(!_finished)) {
        _finished = true;
        qCDebug(lcConnectionValidator) << status << _duration.duration();
        Q_EMIT connectionResult(status, _errors);
        deleteLater();
    }
}

} // namespace OCC

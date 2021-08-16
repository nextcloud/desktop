/*
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

#include "application.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "account.h"
#include "creds/abstractcredentials.h"
#include "creds/httpcredentials.h"
#include "logger.h"
#include "configfile.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <qfontmetrics.h>

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountState, "gui.account.state", QtInfoMsg)

void AccountState::updateUrlDialog(const QUrl &newUrl)
{
    // guard to prevent multiple dialogs
    if (_updateUrlDialog) {
        return;
    }
    auto accept = [=] {
        _account->setUrl(newUrl);
        Q_EMIT _account->wantsAccountSaved(_account.data());
        Q_EMIT urlUpdated();
    };

    auto matchUrl = [](QUrl url1, QUrl url2) {
        // ensure https://demo.owncloud.org/ matches https://demo.owncloud.org
        // the empty path was the legacy formating before 2.9
        if (url1.path().isEmpty()) {
            url1.setPath(QStringLiteral("/"));
        }
        if (url2.path().isEmpty()) {
            url2.setPath(QStringLiteral("/"));
        }
        return url1.matches(url2, QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
    };

    if (matchUrl(newUrl, _account->url())) {
        accept();
        return;
    }

    _updateUrlDialog = new QMessageBox(QMessageBox::Warning, tr("Url update requested for %1").arg(_account->displayName()),
        tr("The url for %1 changed from %2 to %3, do you want to accept the changed url?").arg(_account->displayName(), _account->url().toString(), newUrl.toString()),
        QMessageBox::NoButton, ocApp()->gui()->settingsDialog());
    _updateUrlDialog->setAttribute(Qt::WA_DeleteOnClose);
    auto yes = _updateUrlDialog->addButton(tr("Change url permanently to %1").arg(newUrl.toString()), QMessageBox::AcceptRole);
    _updateUrlDialog->addButton(tr("Reject"), QMessageBox::RejectRole);
    connect(_updateUrlDialog, &QMessageBox::finished, _account.data(), [yes, accept, this] {
        if (_updateUrlDialog->clickedButton() == yes) {
            accept();
        }
    });
    _updateUrlDialog->show();
}

AccountState::AccountState(AccountPtr account)
    : QObject()
    , _account(account)
    , _queueGuard(_account->jobQueue())
    , _state(AccountState::Disconnected)
    , _connectionStatus(ConnectionValidator::Undefined)
    , _waitingForNewCredentials(false)
    , _maintenanceToConnectedDelay(60000 + (qrand() % (4 * 60000))) // 1-5min delay
{
    qRegisterMetaType<AccountState *>("AccountState*");

    connect(account.data(), &Account::invalidCredentials,
        this, &AccountState::slotInvalidCredentials);
    connect(account.data(), &Account::credentialsFetched,
        this, &AccountState::slotCredentialsFetched);
    connect(account.data(), &Account::credentialsAsked,
        this, &AccountState::slotCredentialsAsked);
    connect(account.data(), &Account::unknownConnectionState,
        this, [this] {
            checkConnectivity(true);
        });
    connect(account.data(), &Account::requestUrlUpdate, this, &AccountState::updateUrlDialog);
    connect(this, &AccountState::urlUpdated, this, [this] {
        checkConnectivity(false);
    });
}

AccountState::~AccountState()
{
}

AccountState *AccountState::loadFromSettings(AccountPtr account, QSettings & /*settings*/)
{
    auto accountState = new AccountState(account);
    return accountState;
}

void AccountState::writeToSettings(QSettings & /*settings*/)
{
}

AccountPtr AccountState::account() const
{
    return _account;
}

AccountState::ConnectionStatus AccountState::connectionStatus() const
{
    return _connectionStatus;
}

QStringList AccountState::connectionErrors() const
{
    return _connectionErrors;
}

AccountState::State AccountState::state() const
{
    return _state;
}

void AccountState::setState(State state)
{
    const State oldState = _state;
    if (_state != state) {
        qCInfo(lcAccountState) << "AccountState state change: "
                               << stateString(_state) << "->" << stateString(state);
        _state = state;

        if (_state == SignedOut) {
            _connectionStatus = ConnectionValidator::Undefined;
            _connectionErrors.clear();
        } else if (oldState == SignedOut && _state == Disconnected) {
            // If we stop being voluntarily signed-out, try to connect and
            // auth right now!
            checkConnectivity();
        } else if (_state == ServiceUnavailable) {
            // Check if we are actually down for maintenance.
            // To do this we must clear the connection validator that just
            // produced the 503. It's finished anyway and will delete itself.
            _connectionValidator.clear();
            checkConnectivity();
        }
        if (oldState == Connected || _state == Connected) {
            emit isConnectedChanged();
        }
    }

    // might not have changed but the underlying _connectionErrors might have
    if (_state == Connected) {
        QTimer::singleShot(0, this, [this] {
            // ensure the connection validator is done
            _queueGuard.unblock();
        });
    } else {
        _queueGuard.clear();
    }
    // don't anounce a state change from connected to connected
    // https://github.com/owncloud/client/commit/2c6c21d7532f0cbba4b768fde47810f6673ed931
    if (oldState != state || state != Connected) {
        emit stateChanged(_state);
    }
}

QString AccountState::stateString(State state)
{
    switch (state) {
    case SignedOut:
        return tr("Signed out");
    case Disconnected:
        return tr("Disconnected");
    case Connected:
        return tr("Connected");
    case ServiceUnavailable:
        return tr("Service unavailable");
    case MaintenanceMode:
        return tr("Maintenance mode");
    case NetworkError:
        return tr("Network error");
    case ConfigurationError:
        return tr("Configuration error");
    case AskingCredentials:
        return tr("Asking Credentials");
    }
    return tr("Unknown account state");
}

bool AccountState::isSignedOut() const
{
    return _state == SignedOut;
}

void AccountState::signOutByUi()
{
    account()->credentials()->forgetSensitiveData();
    account()->clearCookieJar();
    setState(SignedOut);
}

void AccountState::freshConnectionAttempt()
{
    if (isConnected())
        setState(Disconnected);
    checkConnectivity();
}

void AccountState::signIn()
{
    if (_state == SignedOut) {
        _waitingForNewCredentials = false;
        setState(Disconnected);
    }
}

bool AccountState::isConnected() const
{
    return _state == Connected;
}

void AccountState::tagLastSuccessfullETagRequest(const QDateTime &tp)
{
    _timeOfLastETagCheck = tp;
}

void AccountState::checkConnectivity(bool blockJobs)
{
    qCWarning(lcAccountState) << "checkConnectivity blocking:" << blockJobs;
    if (isSignedOut() || _waitingForNewCredentials) {
        return;
    }

    if (_connectionValidator && blockJobs && !_queueGuard.queue()->isBlocked()) {
        // abort already running non blocking validator
        _connectionValidator->deleteLater();
        _connectionValidator.clear();
    }
    if (_connectionValidator) {
        qCWarning(lcAccountState) << "ConnectionValidator already running, ignoring" << account()->displayName();
        return;
    }

    // If we never fetched credentials, do that now - otherwise connection attempts
    // make little sense.
    if (!account()->credentials()->wasFetched()) {
        _waitingForNewCredentials = true;
        account()->credentials()->fetchFromKeychain();
        return;
    }

    // IF the account is connected the connection check can be skipped
    // if the last successful etag check job is not so long ago.
    const auto pta = account()->capabilities().remotePollInterval();
    const auto polltime = std::chrono::duration_cast<std::chrono::seconds>(ConfigFile().remotePollInterval(pta));
    const auto elapsed = _timeOfLastETagCheck.secsTo(QDateTime::currentDateTimeUtc());
    if (!blockJobs && isConnected() && _timeOfLastETagCheck.isValid()
        && elapsed <= polltime.count()) {
        qCDebug(lcAccountState) << account()->displayName() << "The last ETag check succeeded within the last " << polltime.count() << "s (" << elapsed << "s). No connection check needed!";
        return;
    }

    if (blockJobs) {
        _queueGuard.block();
    }
    _connectionValidator = new ConnectionValidator(account());
    connect(_connectionValidator, &ConnectionValidator::connectionResult,
        this, &AccountState::slotConnectionValidatorResult);
    if (isConnected()) {
        // Use a small authed propfind as a minimal ping when we're
        // already connected.
        if (blockJobs) {
            if (Theme::instance()->connectionValidatorClearCookies()) {
                // clear the cookies directly before we try to validate
                connect(_connectionValidator, &ConnectionValidator::aboutToStart, _account.get(), &Account::clearCookieJar, Qt::DirectConnection);
            }
            _connectionValidator->checkServer();
        } else {
            _connectionValidator->checkServerAndUpdate();
        }
    } else {
        // Check the server and then the auth.

        // Let's try this for all OS and see if it fixes the Qt issues we have on Linux  #4720 #3888 #4051
        //#ifdef Q_OS_WIN
        // There seems to be a bug in Qt on Windows where QNAM sometimes stops
        // working correctly after the computer woke up from sleep. See #2895 #2899
        // and #2973.
        // As an attempted workaround, reset the QNAM regularly if the account is
        // disconnected.
        account()->resetNetworkAccessManager();

        // If we don't reset the ssl config a second CheckServerJob can produce a
        // ssl config that does not have a sensible certificate chain.
        account()->setSslConfiguration(QSslConfiguration());
        //#endif
        _connectionValidator->checkServerAndUpdate();
    }
}

void AccountState::slotConnectionValidatorResult(ConnectionValidator::Status status, const QStringList &errors)
{
    if (isSignedOut()) {
        qCWarning(lcAccountState) << "Signed out, ignoring" << status << _account->url().toString();
        return;
    }

    // Come online gradually from 503 or maintenance mode
    if (status == ConnectionValidator::Connected
        && (_connectionStatus == ConnectionValidator::ServiceUnavailable
               || _connectionStatus == ConnectionValidator::MaintenanceMode)) {
        if (!_timeSinceMaintenanceOver.isValid()) {
            qCInfo(lcAccountState) << "AccountState reconnection: delaying for"
                                   << _maintenanceToConnectedDelay << "ms";
            _timeSinceMaintenanceOver.start();
            QTimer::singleShot(_maintenanceToConnectedDelay + 100, this, [this] { AccountState::checkConnectivity(false); });
            return;
        } else if (_timeSinceMaintenanceOver.elapsed() < _maintenanceToConnectedDelay) {
            qCInfo(lcAccountState) << "AccountState reconnection: only"
                                   << _timeSinceMaintenanceOver.elapsed() << "ms have passed";
            return;
        }
    }

    if (_connectionStatus != status) {
        qCInfo(lcAccountState) << "AccountState connection status change: "
                               << _connectionStatus << "->"
                               << status;
        _connectionStatus = status;
    }
    _connectionErrors = errors;

    switch (status) {
    case ConnectionValidator::Connected:
        setState(Connected);
        break;
    case ConnectionValidator::Undefined:
    case ConnectionValidator::NotConfigured:
        setState(Disconnected);
        break;
    case ConnectionValidator::ServerVersionMismatch:
        setState(ConfigurationError);
        break;
    case ConnectionValidator::StatusNotFound:
        // This can happen either because the server does not exist
        // or because we are having network issues. The latter one is
        // much more likely, so keep trying to connect.
        setState(NetworkError);
        break;
    case ConnectionValidator::CredentialsWrong:
    case ConnectionValidator::CredentialsNotReady:
        slotInvalidCredentials();
        break;
    case ConnectionValidator::SslError:
        setState(SignedOut);
        break;
    case ConnectionValidator::ServiceUnavailable:
        _timeSinceMaintenanceOver.invalidate();
        setState(ServiceUnavailable);
        break;
    case ConnectionValidator::MaintenanceMode:
        _timeSinceMaintenanceOver.invalidate();
        setState(MaintenanceMode);
        break;
    case ConnectionValidator::Timeout:
        setState(NetworkError);
        break;
    }
}

void AccountState::slotInvalidCredentials()
{
    if (isSignedOut() || _waitingForNewCredentials)
        return;

    qCInfo(lcAccountState) << "Invalid credentials for" << _account->url().toString()
                           << "asking user";

    _waitingForNewCredentials = true;
    setState(AskingCredentials);

    if (account()->credentials()->ready()) {
        account()->credentials()->invalidateToken();
    }
    if (auto creds = qobject_cast<HttpCredentials *>(account()->credentials())) {
        if (creds->refreshAccessToken())
            return;
    }
    account()->credentials()->askFromUser();
}

void AccountState::slotCredentialsFetched(AbstractCredentials *)
{
    // Make a connection attempt, no matter whether the credentials are
    // ready or not - we want to check whether we can get an SSL connection
    // going before bothering the user for a password.
    qCInfo(lcAccountState) << "Fetched credentials for" << _account->url().toString()
                           << "attempting to connect";
    _waitingForNewCredentials = false;
    checkConnectivity();
}

void AccountState::slotCredentialsAsked(AbstractCredentials *credentials)
{
    qCInfo(lcAccountState) << "Credentials asked for" << _account->url().toString()
                           << "are they ready?" << credentials->ready();

    _waitingForNewCredentials = false;

    if (!credentials->ready()) {
        // User canceled the connection or did not give a password
        setState(SignedOut);
        return;
    }

    if (_connectionValidator) {
        // When new credentials become available we always want to restart the
        // connection validation, even if it's currently running.
        _connectionValidator->deleteLater();
        _connectionValidator = nullptr;
    }

    checkConnectivity();
}

std::unique_ptr<QSettings> AccountState::settings()
{
    auto s = ConfigFile::settingsWithGroup(QLatin1String("Accounts"));
    s->beginGroup(_account->id());
    return s;
}

} // namespace OCC

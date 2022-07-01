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

#include "accountstate.h"
#include "account.h"
#include "accountmanager.h"
#include "application.h"
#include "configfile.h"
#include "creds/abstractcredentials.h"
#include "creds/httpcredentials.h"
#include "gui/settingsdialog.h"
#include "gui/tlserrordialog.h"
#include "logger.h"
#include "settingsdialog.h"
#include "socketapi/socketapi.h"
#include "theme.h"

#include <QFontMetrics>
#include <QRandomGenerator>
#include <QSettings>
#include <QTimer>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

inline const QLatin1String userExplicitlySignedOutC()
{
    return QLatin1String("userExplicitlySignedOut");
}

} // anonymous namespace

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountState, "gui.account.state", QtInfoMsg)

// Returns the dialog when one is shown, so callers can attach to signals. If no dialog is shown
// (because there is one already, or the new URL matches the current URL), a nullptr is returned.
UpdateUrlDialog *AccountState::updateUrlDialog(const QUrl &newUrl)
{
    // guard to prevent multiple dialogs
    if (_updateUrlDialog) {
        return nullptr;
    }

    _updateUrlDialog = UpdateUrlDialog::fromAccount(_account, newUrl, ocApp()->activeWindow());

    connect(_updateUrlDialog, &UpdateUrlDialog::accepted, this, [=]() {
        _account->setUrl(newUrl);
        Q_EMIT _account->wantsAccountSaved(_account.data());
        Q_EMIT urlUpdated();
    });

    _updateUrlDialog->show();

    return _updateUrlDialog;
}

AccountState::AccountState(AccountPtr account)
    : QObject()
    , _account(account)
    , _queueGuard(_account->jobQueue())
    , _state(AccountState::Disconnected)
    , _connectionStatus(ConnectionValidator::Undefined)
    , _waitingForNewCredentials(false)
    , _maintenanceToConnectedDelay(1min + minutes(QRandomGenerator::global()->generate() % 4)) // 1-5min delay
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
    connect(account.data(), &Account::requestUrlUpdate, this, &AccountState::updateUrlDialog, Qt::QueuedConnection);
    connect(
        this, &AccountState::urlUpdated, this, [this] {
            checkConnectivity(false);
        },
        Qt::QueuedConnection);

    connect(account->credentials(), &AbstractCredentials::requestLogout, this, [this] {
        _state = State::SignedOut;
    });

    FolderMan::instance()->socketApi()->registerAccount(account);
}

AccountState::~AccountState()
{
    FolderMan::instance()->socketApi()->unregisterAccount(account());
}

AccountStatePtr AccountState::loadFromSettings(AccountPtr account, const QSettings &settings)
{
    auto accountState = AccountStatePtr(new AccountState(account));
    const bool userExplicitlySignedOut = settings.value(userExplicitlySignedOutC(), false).toBool();
    if (userExplicitlySignedOut) {
        // see writeToSettings below
        accountState->_state = SignedOut;
    }
    return accountState;
}

AccountStatePtr AccountState::fromNewAccount(AccountPtr account)
{
    return AccountStatePtr(new AccountState(account));
}

void AccountState::writeToSettings(QSettings &settings) const
{
    // The SignedOut state is the only state where the client should *not* ask for credentials, nor
    // try to connect to the server. All other states should transition to Connected by either
    // (re-)trying to make a connection, or by authenticating (AskCredentials). So we save the
    // SignedOut state to indicate that the client should not try to re-connect the next time it
    // is started.
    settings.setValue(userExplicitlySignedOutC(), _state == SignedOut);
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
    if (_tlsDialog) {
        qCDebug(lcAccountState) << "Skip checkConnectivity, waiting for tls dialog";
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
    }
    if (account()->hasCapabilities()) {
        // IF the account is connected the connection check can be skipped
        // if the last successful etag check job is not so long ago.
        const auto pta = account()->capabilities().remotePollInterval();
        const auto polltime = duration_cast<seconds>(ConfigFile().remotePollInterval(pta));
        const auto elapsed = _timeOfLastETagCheck.secsTo(QDateTime::currentDateTimeUtc());
        if (!blockJobs && isConnected() && _timeOfLastETagCheck.isValid()
            && elapsed <= polltime.count()) {
            qCDebug(lcAccountState) << account()->displayName() << "The last ETag check succeeded within the last " << polltime.count() << "s (" << elapsed << "s). No connection check needed!";
            return;
        }
    }

    if (blockJobs) {
        _queueGuard.block();
    }
    _connectionValidator = new ConnectionValidator(account());
    connect(_connectionValidator, &ConnectionValidator::connectionResult,
        this, &AccountState::slotConnectionValidatorResult);

    connect(_connectionValidator, &ConnectionValidator::sslErrors, this, [blockJobs, this](const QList<QSslError> &errors) {
        if (!_tlsDialog) {
            // ignore errors for already accepted certificates
            auto filteredErrors = _account->accessManager()->filterSslErrors(errors);
            if (!filteredErrors.isEmpty()) {
                _tlsDialog = new TlsErrorDialog(filteredErrors, _account->url().host(), ocApp()->gui()->settingsDialog());
                _tlsDialog->setAttribute(Qt::WA_DeleteOnClose);
                QSet<QSslCertificate> certs;
                certs.reserve(filteredErrors.size());
                for (const auto &error : filteredErrors) {
                    certs << error.certificate();
                }
                connect(_tlsDialog, &TlsErrorDialog::accepted, _tlsDialog, [certs, blockJobs, this]() {
                    _account->addApprovedCerts(certs);
                    _tlsDialog.clear();
                    _waitingForNewCredentials = false;
                    checkConnectivity(blockJobs);
                });
                connect(_tlsDialog, &TlsErrorDialog::rejected, this, [certs, this]() {
                    setState(SignedOut);
                });

                _tlsDialog->show();
            }
        }
        if (_tlsDialog) {
            ocApp()->gui()->raiseDialog(_tlsDialog);
        }
    });
    ConnectionValidator::ValidationMode mode = ConnectionValidator::ValidationMode::ValidateAuthAndUpdate;
    if (isConnected()) {
        // Use a small authed propfind as a minimal ping when we're
        // already connected.
        if (blockJobs) {
            _connectionValidator->setClearCookies(true);
            mode = ConnectionValidator::ValidationMode::ValidateAuth;
        } else {
            mode = ConnectionValidator::ValidationMode::ValidateAuthAndUpdate;
        }
    } else {
        // Check the server and then the auth.
        if (_waitingForNewCredentials) {
            mode = ConnectionValidator::ValidationMode::ValidateServer;
        } else {
            _connectionValidator->setClearCookies(true);
            mode = ConnectionValidator::ValidationMode::ValidateAuthAndUpdate;
        }
    }
    _connectionValidator->checkServer(mode);
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
                                   << _maintenanceToConnectedDelay.count() << "ms";
            _timeSinceMaintenanceOver.start();
            QTimer::singleShot(_maintenanceToConnectedDelay + 100ms, this, [this] { AccountState::checkConnectivity(false); });
            return;
        } else if (_timeSinceMaintenanceOver.elapsed() < _maintenanceToConnectedDelay.count()) {
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
        // handled with the tlsDialog
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
    if (!_waitingForNewCredentials) {
        qCInfo(lcAccountState) << "Invalid credentials for" << _account->url().toString();

        _waitingForNewCredentials = true;
        if (account()->credentials()->ready()) {
            account()->credentials()->invalidateToken();
        }
        if (auto creds = qobject_cast<HttpCredentials *>(account()->credentials())) {
            qCInfo(lcAccountState) << "refreshing oauth";
            if (creds->refreshAccessToken()) {
                return;
            }
            qCInfo(lcAccountState) << "refreshing oauth failed";
        }
        qCInfo(lcAccountState) << "asking user";
        account()->credentials()->askFromUser();
    }
    setState(AskingCredentials);
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

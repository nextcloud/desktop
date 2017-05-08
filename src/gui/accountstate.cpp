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
#include "accountmanager.h"
#include "account.h"
#include "creds/abstractcredentials.h"
#include "logger.h"
#include "configfile.h"

#include <QDebug>
#include <QSettings>
#include <qfontmetrics.h>

namespace OCC {

AccountState::AccountState(AccountPtr account)
    : QObject()
    , _account(account)
    , _state(AccountState::Disconnected)
    , _connectionStatus(ConnectionValidator::Undefined)
    , _waitingForNewCredentials(false)
{
    qRegisterMetaType<AccountState*>("AccountState*");

    connect(account.data(), SIGNAL(invalidCredentials()),
            SLOT(slotInvalidCredentials()));
    connect(account.data(), SIGNAL(credentialsFetched(AbstractCredentials*)),
            SLOT(slotCredentialsFetched(AbstractCredentials*)));
    connect(account.data(), SIGNAL(credentialsAsked(AbstractCredentials*)),
            SLOT(slotCredentialsAsked(AbstractCredentials*)));
    _timeSinceLastETagCheck.invalidate();
}

AccountState::~AccountState()
{
}

AccountState *AccountState::loadFromSettings(AccountPtr account, QSettings& /*settings*/)
{
    auto accountState = new AccountState(account);
    return accountState;
}

void AccountState::writeToSettings(QSettings& /*settings*/)
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

QString AccountState::connectionStatusString(ConnectionStatus status)
{
    return ConnectionValidator::statusString(status);
}

AccountState::State AccountState::state() const
{
    return _state;
}

void AccountState::setState(State state)
{
    if (_state != state) {
        qDebug() << "AccountState state change: "
                 << stateString(_state) << "->" << stateString(state);
        State oldState = _state;
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
    emit stateChanged(_state);
}

QString AccountState::stateString(State state)
{
    switch (state)
    {
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
    setState(SignedOut);
}

void AccountState::signIn()
{
    if (_state == SignedOut) {
        setState(Disconnected);
    }
}

bool AccountState::isConnected() const
{
    return _state == Connected;
}

void AccountState::tagLastSuccessfullETagRequest()
{
    _timeSinceLastETagCheck.restart();
}

void AccountState::checkConnectivity()
{
    if (isSignedOut() || _waitingForNewCredentials) {
        return;
    }

    if (_connectionValidator) {
        qDebug() << "ConnectionValidator already running, ignoring" << account()->displayName();
        return;
    }

    // IF the account is connected the connection check can be skipped
    // if the last successful etag check job is not so long ago.
    ConfigFile cfg;
    int polltime = cfg.remotePollInterval();

    if (isConnected() && _timeSinceLastETagCheck.isValid()
            && _timeSinceLastETagCheck.elapsed() < polltime) {
        //qDebug() << account()->displayName() << "The last ETag check succeeded within the last " << polltime/1000 << " secs. No connection check needed!";
        return;
    }

    ConnectionValidator * conValidator = new ConnectionValidator(account());
    _connectionValidator = conValidator;
    connect(conValidator, SIGNAL(connectionResult(ConnectionValidator::Status,QStringList)),
            SLOT(slotConnectionValidatorResult(ConnectionValidator::Status,QStringList)));
    if (isConnected()) {
        // Use a small authed propfind as a minimal ping when we're
        // already connected.
        conValidator->checkAuthentication();
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
        conValidator->checkServerAndAuth();
    }
}

void AccountState::slotConnectionValidatorResult(ConnectionValidator::Status status, const QStringList& errors)
{
    if (isSignedOut()) {
        qDebug() << "Signed out, ignoring" << connectionStatusString(status) << _account->url().toString();
        return;
    }

    if (_connectionStatus != status) {
        qDebug() << "AccountState connection status change: "
                 << connectionStatusString(_connectionStatus) << "->"
                 << connectionStatusString(status);
        _connectionStatus = status;
    }
    _connectionErrors = errors;

    switch (status)
    {
    case ConnectionValidator::Connected:
        if (_state != Connected) {
            setState(Connected);
        }
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
    case ConnectionValidator::CredentialsMissingOrWrong:
        slotInvalidCredentials();
        break;
    case ConnectionValidator::UserCanceledCredentials:
        setState(SignedOut);
        break;
    case ConnectionValidator::ServiceUnavailable:
        setState(ServiceUnavailable);
        break;
    case ConnectionValidator::MaintenanceMode:
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

    if (account()->credentials()->ready())
        account()->credentials()->invalidateToken();
    account()->credentials()->fetchFromKeychain();

    setState(ConfigurationError);
    _waitingForNewCredentials = true;
}

void AccountState::slotCredentialsFetched(AbstractCredentials* credentials)
{
    if (!credentials->ready()) {
        // No exiting credentials found in the keychain
        credentials->askFromUser();
        return;
    }

    _waitingForNewCredentials = false;

    if (_connectionValidator) {
        // When new credentials become available we always want to restart the
        // connection validation, even if it's currently running.
        _connectionValidator->deleteLater();
        _connectionValidator = 0;
    }

    checkConnectivity();
}

void AccountState::slotCredentialsAsked(AbstractCredentials* credentials)
{
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
        _connectionValidator = 0;
    }

    checkConnectivity();
}

std::unique_ptr<QSettings> AccountState::settings()
{
    auto s = Utility::settingsWithGroup(QLatin1String("Accounts"));
    s->beginGroup(_account->id());
    return s;
}

QString AccountState::shortDisplayNameForSettings(int width) const
{
    QString user = account()->credentials()->user();
    QString host = account()->url().host();
    int port = account()->url().port();
    if (port > 0 && port != 80 && port != 443) {
        host.append(QLatin1Char(':'));
        host.append(QString::number(port));
    }
    if (width > 0) {
        QFont f;
        QFontMetrics fm(f);
        host = fm.elidedText(host, Qt::ElideMiddle, width);
        user = fm.elidedText(user, Qt::ElideRight, width);
    }
    return user + QLatin1String("\n") + host;
}



} // namespace OCC

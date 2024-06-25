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
#include "remotewipe.h"
#include "account.h"
#include "creds/abstractcredentials.h"
#include "creds/httpcredentials.h"
#include "logger.h"
#include "configfile.h"
#include "ocsnavigationappsjob.h"
#include "ocsuserstatusconnector.h"
#include "pushnotifications.h"
#include "networkjobs.h"

#include <QSettings>
#include <QTimer>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QBuffer>
#include <QRandomGenerator>

#include <cmath>

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountState, "nextcloud.gui.account.state", QtInfoMsg)

AccountState::AccountState(const AccountPtr &account)
    : QObject()
    , _account(account)
    , _state(AccountState::Disconnected)
    , _connectionStatus(ConnectionValidator::Undefined)
    , _waitingForNewCredentials(false)
    , _maintenanceToConnectedDelay(60000 + (QRandomGenerator::global()->generate() % (4 * 60000))) // 1-5min delay
    , _remoteWipe(new RemoteWipe(_account))
    , _isDesktopNotificationsAllowed(true)
{
    qRegisterMetaType<AccountState *>("AccountState*");

    connect(account.data(), &Account::invalidCredentials,
        this, &AccountState::slotHandleRemoteWipeCheck);
    connect(account.data(), &Account::credentialsFetched,
        this, &AccountState::slotCredentialsFetched);
    connect(account.data(), &Account::credentialsAsked,
        this, &AccountState::slotCredentialsAsked);
    connect(account.data(), &Account::pushNotificationsReady,
            this, &AccountState::slotPushNotificationsReady);
    connect(account.data(), &Account::serverUserStatusChanged, this,
        &AccountState::slotServerUserStatusChanged);

    connect(this, &AccountState::isConnectedChanged, [=]{
        // Get the Apps available on the server if we're now connected.
        if (isConnected()) {
            fetchNavigationApps();
        }
    });

    connect(&_checkConnectionTimer, &QTimer::timeout, this, &AccountState::slotCheckConnection);
    _checkConnectionTimer.setInterval(ConnectionValidator::DefaultCallingIntervalMsec);
    _checkConnectionTimer.start();

    connect(&_checkServerAvailibilityTimer, &QTimer::timeout, this, &AccountState::slotCheckServerAvailibility);
    _checkServerAvailibilityTimer.setInterval(ConnectionValidator::DefaultCallingIntervalMsec);
    _checkServerAvailibilityTimer.start();

    QTimer::singleShot(0, this, &AccountState::slotCheckConnection);
}

AccountState::~AccountState() = default;

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
    if (_state != state) {
        qCInfo(lcAccountState) << "AccountState state change: "
                               << stateString(_state) << "->" << stateString(state);
        State oldState = _state;
        _state = state;

        if (_state == SignedOut) {
            _connectionStatus = ConnectionValidator::Undefined;
        } else if (oldState == SignedOut && _state == Disconnected) {
            // If we stop being voluntarily signed-out, try to connect and
            // auth right now!
            checkConnectivity();
        } else if (_state == ServiceUnavailable || _state == RedirectDetected) {
            // Check if we are actually down for maintenance/in a redirect state (captive portal?).
            // To do this we must clear the connection validator that just
            // produced the 503/302. It's finished anyway and will delete itself.
            _connectionValidator.clear();
            checkConnectivity();
        }
        if (oldState == Connected || _state == Connected) {
            emit isConnectedChanged();
        }
        if (_state == Connected) {
            resetRetryCount();
        }
    }

    // might not have changed but the underlying _connectionErrors might have
    emit stateChanged(_state);
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
    case RedirectDetected:
        return tr("Redirect detected");
    case NetworkError:
        return tr("Network error");
    case ConfigurationError:
        return tr("Configuration error");
    case AskingCredentials:
        return tr("Asking Credentials");
    }
    return tr("Unknown account state");
}

int AccountState::retryCount() const
{
    return _retryCount;
}

void AccountState::increaseRetryCount()
{
    ++_retryCount;
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

QByteArray AccountState::notificationsEtagResponseHeader() const
{
    return _notificationsEtagResponseHeader;
}

void AccountState::setNotificationsEtagResponseHeader(const QByteArray &value)
{
    _notificationsEtagResponseHeader = value;
}

QByteArray AccountState::navigationAppsEtagResponseHeader() const
{
    return _navigationAppsEtagResponseHeader;
}

void AccountState::setNavigationAppsEtagResponseHeader(const QByteArray &value)
{
    _navigationAppsEtagResponseHeader = value;
}

bool AccountState::isDesktopNotificationsAllowed() const
{
    return _isDesktopNotificationsAllowed;
}

void AccountState::setDesktopNotificationsAllowed(bool isAllowed)
{
    if (_isDesktopNotificationsAllowed == isAllowed) {
        return;
    }
    
    _isDesktopNotificationsAllowed = isAllowed;
    emit desktopNotificationsAllowedChanged();
}

AccountState::ConnectionStatus AccountState::lastConnectionStatus() const
{
    return _lastConnectionValidatorStatus;
}

void AccountState::trySignIn()
{
    if (isSignedOut() && account()) {
        account()->resetRejectedCertificates();
        signIn();
    }
}

void AccountState::systemOnlineConfigurationChanged()
{
    QMetaObject::invokeMethod(this, "slotCheckConnection", Qt::QueuedConnection);
}

void AccountState::checkConnectivity()
{
    qCInfo(lcAccountState()) << "check connectivity";

    if (isSignedOut() || _waitingForNewCredentials) {
        return;
    }

    if (_connectionValidator) {
        qCWarning(lcAccountState) << "ConnectionValidator already running, ignoring" << account()->displayName();
        return;
    }

    // If we never fetched credentials, do that now - otherwise connection attempts
    // make little sense, we might be missing client certs.
    if (!account()->credentials()->wasFetched()) {
        _waitingForNewCredentials = true;
        account()->credentials()->fetchFromKeychain();
        return;
    }

    // IF the account is connected the connection check can be skipped
    // if the last successful etag check job is not so long ago.
    const auto polltime = std::chrono::duration_cast<std::chrono::seconds>(ConfigFile().remotePollInterval());
    const auto elapsed = _timeOfLastETagCheck.secsTo(QDateTime::currentDateTimeUtc());
    if (isConnected() && _timeOfLastETagCheck.isValid()
        && elapsed <= polltime.count()) {
        qCDebug(lcAccountState) << account()->displayName() << "The last ETag check succeeded within the last " << polltime.count() << "s (" << elapsed << "s). No connection check needed!";
        return;
    }

    auto *conValidator = new ConnectionValidator(AccountStatePtr(this), _connectionErrors);
    _connectionValidator = conValidator;
    _connectionErrors.clear();
    connect(conValidator, &ConnectionValidator::connectionResult,
        this, &AccountState::slotConnectionValidatorResult);
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
        account()->setSslConfiguration(QSslConfiguration::defaultConfiguration());
        //#endif
        conValidator->checkServerAndAuth();
    }
}

void AccountState::slotConnectionValidatorResult(ConnectionValidator::Status status, const QStringList &errors)
{
    const auto updateRetryCount = [this]() {
        increaseRetryCount();
        qCInfo(lcAccountState()) << "connection retry count" << retryCount();
        _lastCheckConnectionTimer.invalidate();
        _lastCheckConnectionTimer.start();
    };

    const auto resetRetryConnection = [this]() {
        qCInfo(lcAccountState) << "reset retry count";
        resetRetryCount();
        _lastCheckConnectionTimer.invalidate();
        _lastCheckConnectionTimer.start();
    };

    if (isSignedOut()) {
        qCWarning(lcAccountState) << "Signed out, ignoring" << status << _account->url().toString();
        return;
    }

    _lastConnectionValidatorStatus = status;

    // Come online gradually from 503, captive portal(redirection) or maintenance mode
    if (status == ConnectionValidator::Connected
        && (_connectionStatus == ConnectionValidator::ServiceUnavailable
            || _connectionStatus == ConnectionValidator::MaintenanceMode
              || _connectionStatus == ConnectionValidator::StatusRedirect)) {
        if (!_timeSinceMaintenanceOver.isValid()) {
            qCInfo(lcAccountState) << "AccountState reconnection: delaying for"
                                   << _maintenanceToConnectedDelay << "ms";
            _timeSinceMaintenanceOver.start();
            QTimer::singleShot(_maintenanceToConnectedDelay + 100, this, &AccountState::checkConnectivity);
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
        emit stateChanged(_state);
    }
    _connectionErrors = errors;

    switch (status) {
    case ConnectionValidator::Connected:
        if (_state != Connected) {
            setState(Connected);
            resetRetryConnection();

            // Get the Apps available on the server.
            fetchNavigationApps();

            // Setup push notifications after a successful connection
            account()->trySetupPushNotifications();
        }
        break;
    case ConnectionValidator::Undefined:
    case ConnectionValidator::NotConfigured:
        setState(Disconnected);
        updateRetryCount();
        break;
    case ConnectionValidator::ServerVersionMismatch:
        setState(ConfigurationError);
        break;
    case ConnectionValidator::StatusNotFound:
        // This can happen either because the server does not exist
        // or because we are having network issues. The latter one is
        // much more likely, so keep trying to connect.
        setState(NetworkError);
        updateRetryCount();
        break;
    case ConnectionValidator::CredentialsWrong:
    case ConnectionValidator::CredentialsNotReady:
        handleInvalidCredentials();
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
    case ConnectionValidator::StatusRedirect:
        _timeSinceMaintenanceOver.invalidate();
        setState(RedirectDetected);
        break;
    case ConnectionValidator::Timeout:
        setState(NetworkError);
        updateRetryCount();
        break;
    }
}

void AccountState::slotHandleRemoteWipeCheck()
{
    // make sure it changes account state and icons
    signOutByUi();

    qCInfo(lcAccountState) << "Invalid credentials for" << _account->url().toString()
                           << "checking for remote wipe request";

    _waitingForNewCredentials = false;
    setState(SignedOut);
}


void AccountState::handleInvalidCredentials()
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

void AccountState::fetchNavigationApps(){
    auto *job = new OcsNavigationAppsJob(_account);
    job->addRawHeader("If-None-Match", navigationAppsEtagResponseHeader());
    connect(job, &OcsNavigationAppsJob::appsJobFinished, this, &AccountState::slotNavigationAppsFetched);
    connect(job, &OcsNavigationAppsJob::etagResponseHeaderReceived, this, &AccountState::slotEtagResponseHeaderReceived);
    connect(job, &OcsNavigationAppsJob::ocsError, this, &AccountState::slotOcsError);
    job->getNavigationApps();
}

void AccountState::resetRetryCount()
{
    _retryCount = 0;
}

void AccountState::slotEtagResponseHeaderReceived(const QByteArray &value, int statusCode){
    if(statusCode == 200){
        qCDebug(lcAccountState) << "New navigation apps ETag Response Header received " << value;
        setNavigationAppsEtagResponseHeader(value);
    }
}

void AccountState::slotOcsError(int statusCode, const QString &message)
{
    qCDebug(lcAccountState) << "Error " << statusCode << " while fetching new navigation apps: " << message;
}

void AccountState::slotCheckConnection()
{
    if (_lastCheckConnectionTimer.isValid()) {
        static constexpr auto DefaultCallingIntervalMaxMsec = static_cast<int>(ConnectionValidator::DefaultCallingIntervalMsec) * 8;

        const auto minDelay = std::max(retryCount() * ConnectionValidator::DefaultCallingIntervalMsec,
                                       static_cast<int>(ConnectionValidator::DefaultCallingIntervalMsec));
        const auto currentDelay = std::min(minDelay, DefaultCallingIntervalMaxMsec);

        if (!_lastCheckConnectionTimer.hasExpired(currentDelay - 1)) {
            qCInfo(lcAccountState()) << "timer has not expired: do not check now" << _lastCheckConnectionTimer.elapsed() << currentDelay;
            return;
        }
    }

    const auto currentState = state();

    // Don't check if we're manually signed out or
    // when the error is permanent.
    const auto pushNotifications = account()->pushNotifications();
    const auto pushNotificationsAvailable = (pushNotifications && pushNotifications->isReady());
    if (currentState != AccountState::SignedOut && currentState != AccountState::ConfigurationError
        && currentState != AccountState::AskingCredentials && !pushNotificationsAvailable) {
        checkConnectivity();
    } else if (currentState == AccountState::SignedOut && lastConnectionStatus() == AccountState::ConnectionStatus::SslError) {
        qCWarning(lcAccountState()) << "Account is signed out due to SSL Handshake error. Going to perform a sign-in attempt...";
        trySignIn();
    }
}

void AccountState::slotCheckServerAvailibility()
{
    if (state() == AccountState::Connected
        || state() == AccountState::SignedOut
        || state() == AccountState::MaintenanceMode
        || state() == AccountState::AskingCredentials) {
        qCInfo(lcAccountState) << "Skipping server availability check for account" << _account->davUser() << "with state" << state();
        return;
    }
    qCInfo(lcAccountState) << "Checking server availability for account" << _account->davUser();
    const auto serverAvailibilityUrl = Utility::concatUrlPath(_account->url(), QLatin1String("/index.php/204"));
    auto checkServerAvailibilityJob = _account->sendRequest(QByteArrayLiteral("GET"), serverAvailibilityUrl);
    connect(checkServerAvailibilityJob, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 204) {
            qCInfo(lcAccountState) << "Server is now available for account" << _account->davUser();
            _lastCheckConnectionTimer.invalidate();
            resetRetryCount();
            QMetaObject::invokeMethod(this, &AccountState::slotCheckConnection, Qt::QueuedConnection);
        }
    });
}

void AccountState::slotPushNotificationsReady()
{
    if (state() != AccountState::State::Connected) {
        setState(AccountState::State::Connected);
    }
}

void AccountState::slotServerUserStatusChanged()
{
    setDesktopNotificationsAllowed(_account->userStatusConnector()->userStatus().state() != UserStatus::OnlineStatus::DoNotDisturb);
}

void AccountState::slotNavigationAppsFetched(const QJsonDocument &reply, int statusCode)
{
    if(_account){
        if (statusCode == 304) {
            qCWarning(lcAccountState) << "Status code " << statusCode << " Not Modified - No new navigation apps.";
        } else {
            _apps.clear();

            if(!reply.isEmpty()){
                auto element = reply.object().value("ocs").toObject().value("data");
                const auto navLinks = element.toArray();

                if(navLinks.size() > 0){
                    for (const QJsonValue &value : navLinks) {
                        auto navLink = value.toObject();

                        auto *app = new AccountApp(navLink.value("name").toString(), QUrl(navLink.value("href").toString()),
                            navLink.value("id").toString(), QUrl(navLink.value("icon").toString()));

                        _apps << app;
                    }
                }
            }

            emit hasFetchedNavigationApps();
        }
    }
}

AccountAppList AccountState::appList() const
{
    return _apps;
}

AccountApp* AccountState::findApp(const QString &appId) const
{
    if(!appId.isEmpty()) {
        const auto apps = appList();
        const auto it = std::find_if(apps.cbegin(), apps.cend(), [appId](const auto &app) {
            return app->id() == appId;
        });
        if (it != apps.cend()) {
            return *it;
        }
    }

    return nullptr;
}

/*-------------------------------------------------------------------------------------*/

AccountApp::AccountApp(const QString &name, const QUrl &url,
    const QString &id, const QUrl &iconUrl,
    QObject *parent)
    : QObject(parent)
    , _name(name)
    , _url(url)
    , _id(id)
    , _iconUrl(iconUrl)
{
}

QString AccountApp::name() const
{
    return _name;
}

QUrl AccountApp::url() const
{
    return _url;
}

QString AccountApp::id() const
{
    return _id;
}

QUrl AccountApp::iconUrl() const
{
    return _iconUrl;
}

/*-------------------------------------------------------------------------------------*/

} // namespace OCC

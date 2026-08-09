/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#include <chrono>
#include <cmath>

using namespace Qt::StringLiterals;

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountState, "nextcloud.gui.account.state", QtInfoMsg)

namespace {

/// How many connection checks must time out back to back before the account is called down.
///
/// A single timeout says only that one request did not come back in time, which a server that
/// stalls briefly under load produces regularly. Acting on it disconnects the account and
/// tears down a sync that was making progress on its own requests, throwing away the whole
/// discovery pass. Requiring several in a row distinguishes "one slow request" from "the
/// server is gone", at the cost of noticing a genuine outage a couple of checks later --
/// which costs nothing, since a sync against a server that is really gone fails on its own
/// request timeouts anyway.
int connectionTimeoutsBeforeDisconnect()
{
    static const auto configured = qEnvironmentVariableIntValue("OWNCLOUD_CONNECTION_TIMEOUT_RETRIES");
    static constexpr auto defaultTolerated = 3;
    return qMax(1, configured > 0 ? configured : defaultTolerated);
}

/// Delay before re-probing after a tolerated timeout. Long enough not to pile another request
/// onto a server that is already struggling, short enough that a real outage is still noticed
/// promptly.
constexpr auto recheckAfterToleratedTimeout = std::chrono::seconds(5);

/// How long a timeout stays "recent" for the purpose of counting consecutive failures.
///
/// Without this the count is consecutive in sequence but not in time, so three failures spread
/// over a quarter of an hour -- a server that stumbled three separate times and recovered in
/// between -- look identical to a server that has stopped answering. Only failures that keep
/// arriving inside this window are evidence of an outage; an isolated one that is followed by a
/// long quiet period is just a stumble, and the count starts over.
///
/// The window cannot be short. A check that times out takes as long as its own timeout to do
/// so -- 60s and upwards, more when the reply trickles and keeps resetting the inactivity timer
/// -- and only then is the next one scheduled. Consecutive failures are therefore minutes apart
/// by construction, and a window below that would reset every time, making the count
/// unreachable and this whole mechanism a no-op.
std::chrono::seconds connectionTimeoutCountResetAfter()
{
    static const auto configured = qEnvironmentVariableIntValue("OWNCLOUD_CONNECTION_TIMEOUT_RESET_SEC");
    static constexpr auto defaultSeconds = 300;
    return std::chrono::seconds(qMax(60, configured > 0 ? configured : defaultSeconds));
}

}

AccountState::AccountState(const AccountPtr &account)
    : QObject()
    , _account(account)
    , _state(AccountState::Disconnected)
    , _connectionStatus(ConnectionValidator::Undefined)
    , _waitingForNewCredentials(false)
    , _termsOfServiceChecker(_account)
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
    connect(&_termsOfServiceChecker, &TermsOfServiceChecker::done,
            this, [this] ()
            {
                if (_termsOfServiceChecker.needToSign()) {
                    slotConnectionValidatorResult(ConnectionValidator::NeedToSignTermsOfService, {});
                }
            });
    connect(account.data(), &Account::termsOfServiceNeedToBeChecked,
            this, [this] ()
            {
                _termsOfServiceChecker.start();
            });

    connect(this, &AccountState::isConnectedChanged, [=, this]{
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
    case NeedToSignTermsOfService:
        return tr("Need the user to accept the terms of service");
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
    if (isConnected()) {
        setState(Disconnected);
    }

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

bool AccountState::needsToSignTermsOfService() const
{
    return _state == NeedToSignTermsOfService;
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
        ConfigFile configFile;
        const auto shouldTryUnbrandedToBrandedMigration = configFile.shouldTryUnbrandedToBrandedMigration();
        qCDebug(lcAccountState) << "shouldTryUnbrandedToBrandedMigration?" << shouldTryUnbrandedToBrandedMigration;
        qCDebug(lcAccountState) << "migrationPhase?" << configFile.migrationPhase();
        const auto appName = shouldTryUnbrandedToBrandedMigration ? configFile.unbrandedAppName : "";
        account()->credentials()->fetchFromKeychain(appName);
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
    if (isConnected() || needsToSignTermsOfService()) {
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

    // Absorb isolated check timeouts. Returning here leaves the account exactly as it was --
    // state, connection status and error list all untouched -- so nothing downstream can tell
    // a tolerated timeout happened, and in particular no running sync is torn down for it.
    if (status == ConnectionValidator::Timeout) {
        // Failures only add up while they keep arriving. One that follows a long quiet spell
        // says the server recovered in between, so it starts a fresh count rather than
        // compounding with something that happened a quarter of an hour ago.
        const auto resetAfter = connectionTimeoutCountResetAfter();
        if (_lastConnectionTimeout.isValid()
            && _lastConnectionTimeout.durationElapsed() > resetAfter) {
            qCInfo(lcAccountState) << "Last connection timeout for" << _account->url().toString()
                                   << "was"
                                   << std::chrono::duration_cast<std::chrono::seconds>(
                                          _lastConnectionTimeout.durationElapsed()).count()
                                   << "s ago, more than the" << resetAfter.count()
                                   << "s window; starting the count over";
            _consecutiveConnectionTimeouts = 0;
        }
        _lastConnectionTimeout.start();

        ++_consecutiveConnectionTimeouts;
        const auto tolerated = connectionTimeoutsBeforeDisconnect();
        if (_consecutiveConnectionTimeouts < tolerated) {
            qCInfo(lcAccountState) << "Connection check timed out for" << _account->url().toString()
                                   << "(" << _consecutiveConnectionTimeouts << "of" << tolerated
                                   << "); staying connected and re-checking";
            // The re-check below is the only thing that will reconsider this account, so it must
            // actually run. checkConnectivity() skips the probe when a recent ETag check
            // succeeded, and we stay Connected here, so that shortcut would otherwise apply and
            // leave the account wedged as Connected however long the server stays down.
            _timeOfLastETagCheck = {};
            QTimer::singleShot(recheckAfterToleratedTimeout, this, &AccountState::checkConnectivity);
            return;
        }
        qCWarning(lcAccountState) << "Connection check timed out" << _consecutiveConnectionTimeouts
                                  << "times in a row for" << _account->url().toString()
                                  << "within" << connectionTimeoutCountResetAfter().count()
                                  << "s of each other; treating the account as disconnected";
    } else {
        // A check that got an answer -- any answer -- ends the run of failures.
        _consecutiveConnectionTimeouts = 0;
        _lastConnectionTimeout.invalidate();
    }

    const auto oldConnectionValidatorStatus = _lastConnectionValidatorStatus;
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
    case ConnectionValidator::NeedToSignTermsOfService:
        setState(NeedToSignTermsOfService);
        break;
    }

    if ((oldConnectionValidatorStatus == ConnectionValidator::NeedToSignTermsOfService && status == ConnectionValidator::Connected) ||
        (status == ConnectionValidator::NeedToSignTermsOfService && oldConnectionValidatorStatus != status)) {

        emit termsOfServiceChanged(_account, status == ConnectionValidator::NeedToSignTermsOfService ? AccountState::NeedToSignTermsOfService : AccountState::Connected);
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
    ConfigFile configFile;
    if (configFile.isMigrationInProgress()) {
        configFile.setMigrationPhase(ConfigFile::MigrationPhase::Done);
    }
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
    qCWarning(lcAccountState) << "Error " << statusCode << " while fetching new navigation apps: " << message;
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

                        auto *app = new AccountApp(navLink.value("name"_L1).toString(), QUrl(navLink.value("href"_L1).toString()),
                            navLink.value("id"_L1).toString(), QUrl(navLink.value("icon"_L1).toString()));

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

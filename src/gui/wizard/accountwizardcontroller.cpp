/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wizard/accountwizardcontroller.h"

#include "account.h"
#include "accountmanager.h"
#include "clientproxy.h"
#include "common/utility.h"
#include "common/vfs.h"
#include "configfile.h"
#include "creds/credentialsfactory.h"
#include "creds/httpcredentialsgui.h"
#include "creds/webflowcredentials.h"
#include "filesystem.h"
#include "folder.h"
#include "folderman.h"
#include "guiutility.h"
#include "networkjobs.h"
#include "owncloudpropagator_p.h"
#include "selectivesyncdialog.h"
#include "theme.h"

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "gui/macOS/fileprovider.h"
#include "gui/macOS/fileprovidersettingscontroller.h"
#endif

#ifdef Q_OS_MACOS
#include "common/utility_mac_sandbox.h"
#endif

#include <QBuffer>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QPointer>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QStorageInfo>
#include <QTimer>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountWizardController, "nextcloud.gui.accountwizardcontroller", QtInfoMsg)

namespace {

bool localFolderContainsData(const QString &localSyncFolder)
{
    const auto localFolder = QDir(localSyncFolder);
    return localFolder.exists()
        && !localFolder.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
}

}

AccountWizardController::AccountWizardController(QObject *parent)
    : QObject(parent)
{
    initialiseAccount();

    ConfigFile cfg;
    const auto largeFolderLimit = cfg.newBigFolderSizeLimit();
    _askBeforeLargeFolders = largeFolderLimit.first;
    _largeFolderThresholdMb = static_cast<int>(largeFolderLimit.second);
    _askBeforeExternalStorage = cfg.confirmExternalStorage();

    if (canUseVirtualFiles()) {
        _syncMode = VirtualFiles;
    }
}

AccountWizardController::~AccountWizardController() = default;

void AccountWizardController::initialiseAccount()
{
    ConfigFile cfg;
    if (!cfg.overrideServerUrl().isEmpty()) {
        Theme::instance()->setOverrideServerUrl(cfg.overrideServerUrl());
        Theme::instance()->setForceOverrideServerUrl(true);
        Theme::instance()->setVfsEnabled(cfg.isVfsEnabled());
        Theme::instance()->setStartLoginFlowAutomatically(true);
    }

    initialiseOverrideServerChoices();

    const auto defaultUrl =
        overrideServerSelectionRequired() ? _overrideServerUrls.value(_overrideServerIndex) :
        Theme::instance()->multipleOverrideServers() ? QString{} : Theme::instance()->overrideServerUrl();

    _remoteFolder = Theme::instance()->defaultServerFolder();
    setServerUrl(defaultUrl);
    const auto hasForcedConcreteServerUrl =
        Theme::instance()->forceOverrideServerUrl() && !defaultUrl.isEmpty() && !Theme::instance()->multipleOverrideServers();
    setServerUrlEditable(!hasForcedConcreteServerUrl && !overrideServerSelectionRequired());
}

void AccountWizardController::initialiseOverrideServerChoices()
{
    if (!Theme::instance()->forceOverrideServerUrl() || !Theme::instance()->multipleOverrideServers()) {
        return;
    }

    const auto serversJsonArray = QJsonDocument::fromJson(Theme::instance()->overrideServerUrl().toUtf8()).array();
    for (const auto &serverJson : serversJsonArray) {
        const auto serverObject = serverJson.toObject();
        const auto serverName = serverObject.value("name"_L1).toString();
        const auto serverUrl = serverObject.value("url"_L1).toString();
        if (serverName.isEmpty() || serverUrl.isEmpty()) {
            continue;
        }
        _overrideServerNames.append(serverName);
        _overrideServerUrls.append(serverUrl);
    }

    if (!_overrideServerUrls.isEmpty()) {
        _overrideServerIndex = 0;
    }
}

void AccountWizardController::ensureAccount()
{
    if (_account) {
        return;
    }

    _account = AccountManager::createAccount();
    _account->setCredentials(CredentialsFactory::create("dummy"));
}

AccountWizardController::Step AccountWizardController::currentStep() const
{
    return _currentStep;
}

QString AccountWizardController::serverUrl() const
{
    return _serverUrl;
}

void AccountWizardController::setServerUrl(const QString &serverUrl)
{
    if (_serverUrl == serverUrl) {
        return;
    }

    _serverUrl = serverUrl;
    emit serverUrlChanged();
    emit proxySettingsChanged();
}

bool AccountWizardController::serverUrlEditable() const
{
    return _serverUrlEditable;
}

bool AccountWizardController::overrideServerSelectionRequired() const
{
    return !_overrideServerUrls.isEmpty();
}

bool AccountWizardController::startLoginFlowAutomatically() const
{
    return Theme::instance()->startLoginFlowAutomatically()
        && Theme::instance()->forceOverrideServerUrl()
        && !_serverUrl.isEmpty()
        && !Theme::instance()->multipleOverrideServers()
        && !overrideServerSelectionRequired();
}

QStringList AccountWizardController::overrideServerNames() const
{
    return _overrideServerNames;
}

int AccountWizardController::overrideServerIndex() const
{
    return _overrideServerIndex;
}

void AccountWizardController::setOverrideServerIndex(int index)
{
    if (index < 0 || index >= _overrideServerUrls.size() || _overrideServerIndex == index) {
        return;
    }

    _overrideServerIndex = index;
    setServerUrl(_overrideServerUrls.at(index));
    emit overrideServerSelectionChanged();
}

bool AccountWizardController::busy() const
{
    return _busy;
}

bool AccountWizardController::authPolling() const
{
    return _authPolling;
}

QString AccountWizardController::errorText() const
{
    return _errorText;
}

QUrl AccountWizardController::loginUrl() const
{
    return _loginUrl;
}

QString AccountWizardController::authStatusText() const
{
    return _authStatusText;
}

QString AccountWizardController::userDisplayName() const
{
    return _userDisplayName;
}

QString AccountWizardController::serverDisplayName() const
{
    return _serverDisplayName;
}

QString AccountWizardController::avatarUrl() const
{
    return _avatarUrl;
}

QString AccountWizardController::syncEverythingDescription() const
{
    return _syncEverythingDescription.isEmpty()
        ? tr("Will require local storage")
        : _syncEverythingDescription;
}

QString AccountWizardController::localSyncFolder() const
{
    return _localSyncFolder;
}

QString AccountWizardController::localSyncFolderDisplay() const
{
    return QDir::toNativeSeparators(_localSyncFolder);
}

QString AccountWizardController::localSyncFolderError() const
{
    return _localSyncFolderError;
}

QString AccountWizardController::localSyncFolderFreeSpace() const
{
    return _localSyncFolderFreeSpace;
}

bool AccountWizardController::localSyncFolderRequired() const
{
#ifdef BUILD_FILE_PROVIDER_MODULE
    return _syncMode != VirtualFiles;
#else
    return true;
#endif
}

AccountWizardController::SyncMode AccountWizardController::syncMode() const
{
    return _syncMode;
}

bool AccountWizardController::canFinish() const
{
    return !localSyncFolderRequired() || _localSyncFolderValid;
}

bool AccountWizardController::canUseVirtualFiles() const
{
    if (Theme::instance()->disableVirtualFilesSyncFolder()) {
        return false;
    }

#ifdef BUILD_FILE_PROVIDER_MODULE
    return Mac::FileProvider::available();
#elif defined(Q_OS_WIN)
    return bestAvailableVfsMode() == Vfs::WindowsCfApi && Theme::instance()->showVirtualFilesOption();
#else
    return bestAvailableVfsMode() != Vfs::Off && Theme::instance()->showVirtualFilesOption();
#endif
}

bool AccountWizardController::isUsingFileProvider() const
{
#ifdef BUILD_FILE_PROVIDER_MODULE
    return Mac::FileProvider::available();
#else
    return false;
#endif
}

bool AccountWizardController::canUseClassicSync() const
{
    return !Theme::instance()->enforceVirtualFilesSyncFolder() || !canUseVirtualFiles();
}

bool AccountWizardController::needsSyncOptions() const
{
    return _needsSyncOptions;
}

bool AccountWizardController::canSkipFolderConfiguration() const
{
    return true;
}

bool AccountWizardController::hasAdvancedOptions() const
{
    return showLargeFolderConfirmation() || showExternalStorageConfirmation();
}

bool AccountWizardController::showLargeFolderConfirmation() const
{
    return !Theme::instance()->wizardHideFolderSizeLimitCheckbox();
}

bool AccountWizardController::askBeforeLargeFolders() const
{
    return _askBeforeLargeFolders;
}

int AccountWizardController::largeFolderThresholdMb() const
{
    return _largeFolderThresholdMb;
}

bool AccountWizardController::showExternalStorageConfirmation() const
{
    return !Theme::instance()->wizardHideExternalStorageConfirmationCheckbox();
}

bool AccountWizardController::askBeforeExternalStorage() const
{
    return _askBeforeExternalStorage;
}

bool AccountWizardController::proxySettingsAvailable() const
{
    return !Theme::instance()->doNotUseProxy();
}

int AccountWizardController::proxyMode() const
{
    switch (_proxySettings._proxyType) {
    case QNetworkProxy::DefaultProxy:
        return 1;
    case QNetworkProxy::HttpProxy:
    case QNetworkProxy::Socks5Proxy:
        return 2;
    case QNetworkProxy::HttpCachingProxy:
    case QNetworkProxy::FtpCachingProxy:
    case QNetworkProxy::NoProxy:
        return 0;
    }

    return 0;
}

void AccountWizardController::setProxyMode(int proxyMode)
{
    const auto previousProxyType = _proxySettings._proxyType;
    switch (proxyMode) {
    case 1:
        _proxySettings._proxyType = QNetworkProxy::DefaultProxy;
        break;
    case 2:
        if (_proxySettings._proxyType != QNetworkProxy::HttpProxy && _proxySettings._proxyType != QNetworkProxy::Socks5Proxy) {
            _proxySettings._proxyType = QNetworkProxy::HttpProxy;
        }
        break;
    default:
        _proxySettings._proxyType = QNetworkProxy::NoProxy;
        break;
    }

    if (_proxySettings._proxyType != previousProxyType) {
        emit proxySettingsChanged();
    }
}

int AccountWizardController::manualProxyType() const
{
    return _proxySettings._proxyType == QNetworkProxy::Socks5Proxy ? 1 : 0;
}

void AccountWizardController::setManualProxyType(int manualProxyType)
{
    if (_proxySettings._proxyType != QNetworkProxy::HttpProxy && _proxySettings._proxyType != QNetworkProxy::Socks5Proxy) {
        return;
    }

    const auto proxyType = manualProxyType == 1 ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy;
    if (_proxySettings._proxyType == proxyType) {
        return;
    }
    _proxySettings._proxyType = proxyType;
    emit proxySettingsChanged();
}

QString AccountWizardController::proxyHost() const
{
    return _proxySettings._host;
}

void AccountWizardController::setProxyHost(const QString &proxyHost)
{
    if (_proxySettings._host == proxyHost) {
        return;
    }
    _proxySettings._host = proxyHost;
    emit proxySettingsChanged();
}

int AccountWizardController::proxyPort() const
{
    return _proxySettings._port;
}

void AccountWizardController::setProxyPort(int proxyPort)
{
    const auto boundedProxyPort = qBound(1, proxyPort, 65535);
    if (_proxySettings._port == boundedProxyPort) {
        return;
    }
    _proxySettings._port = static_cast<quint16>(boundedProxyPort);
    emit proxySettingsChanged();
}

bool AccountWizardController::proxyAuthenticationRequired() const
{
    return _proxySettings._needsAuth == ProxyAuthentication::AuthenticationRequired;
}

void AccountWizardController::setProxyAuthenticationRequired(bool proxyAuthenticationRequired)
{
    const auto proxyAuthentication = proxyAuthenticationRequired
        ? ProxyAuthentication::AuthenticationRequired
        : ProxyAuthentication::NoAuthentication;
    if (_proxySettings._needsAuth == proxyAuthentication) {
        return;
    }
    _proxySettings._needsAuth = proxyAuthentication;
    emit proxySettingsChanged();
}

QString AccountWizardController::proxyUser() const
{
    return _proxySettings._user;
}

void AccountWizardController::setProxyUser(const QString &proxyUser)
{
    if (_proxySettings._user == proxyUser) {
        return;
    }
    _proxySettings._user = proxyUser;
    emit proxySettingsChanged();
}

QString AccountWizardController::proxyPassword() const
{
    return _proxySettings._password;
}

void AccountWizardController::setProxyPassword(const QString &proxyPassword)
{
    if (_proxySettings._password == proxyPassword) {
        return;
    }
    _proxySettings._password = proxyPassword;
    emit proxySettingsChanged();
}

bool AccountWizardController::proxySettingsValid() const
{
    if (proxyMode() != 2) {
        return true;
    }
    if (_proxySettings._host.isEmpty()) {
        return false;
    }
    return !proxyAuthenticationRequired() || (!_proxySettings._user.isEmpty() && !_proxySettings._password.isEmpty());
}

bool AccountWizardController::showProxyLocalhostWarning() const
{
    if (proxyMode() != 2) {
        return false;
    }
    const auto host = QUrl::fromUserInput(_serverUrl).host();
    return host == "localhost"_L1 || host.startsWith("127."_L1) || host == "::1"_L1;
}

QString AccountWizardController::basicAuthUser() const
{
    return _basicAuthUser;
}

void AccountWizardController::setBasicAuthUser(const QString &user)
{
    if (_basicAuthUser == user) {
        return;
    }

    _basicAuthUser = user;
    emit basicAuthChanged();
}

QString AccountWizardController::basicAuthPassword() const
{
    return _basicAuthPassword;
}

void AccountWizardController::setBasicAuthPassword(const QString &password)
{
    if (_basicAuthPassword == password) {
        return;
    }

    _basicAuthPassword = password;
    emit basicAuthChanged();
}

bool AccountWizardController::basicAuthValid() const
{
    return !_basicAuthUser.isEmpty();
}

bool AccountWizardController::publicShareSetup() const
{
    return _publicShareSetup;
}

QString AccountWizardController::appName() const
{
    return Theme::instance()->appNameGUI();
}

QString AccountWizardController::serverUrlPlaceholder() const
{
    return Theme::instance()->wizardUrlHint();
}

QString AccountWizardController::clientCertificatePath() const
{
    return _clientCertificatePath;
}

QString AccountWizardController::clientCertificatePassword() const
{
    return _clientCertificatePassword;
}

void AccountWizardController::setClientCertificatePassword(const QString &password)
{
    if (_clientCertificatePassword == password) {
        return;
    }

    _clientCertificatePassword = password;
    emit clientCertificateChanged();
}

QString AccountWizardController::clientCertificateError() const
{
    return _clientCertificateError;
}

bool AccountWizardController::clientCertificateValid() const
{
    return !_clientCertificatePath.isEmpty();
}

QString AccountWizardController::normalizeServerUrlInput(const QString &serverUrl, const QString &davPath)
{
    auto result = serverUrl.simplified();
    if (!result.isEmpty() && !result.contains("://"_L1)) {
        result.prepend("https://"_L1);
    }
    if (result.endsWith("index.php"_L1)) {
        result.chop(9);
    }

    auto cleanedDavPath = davPath;
    if (!cleanedDavPath.isEmpty() && result.endsWith(cleanedDavPath)) {
        result.chop(cleanedDavPath.length());
    }
    if (cleanedDavPath.endsWith('/'_L1)) {
        cleanedDavPath.chop(1);
        if (!cleanedDavPath.isEmpty() && result.endsWith(cleanedDavPath)) {
            result.chop(cleanedDavPath.length());
        }
    }

    return result;
}

void AccountWizardController::submitServerUrl()
{
    if (_busy) {
        return;
    }

    if (!proxySettingsValid()) {
        setErrorText(tr("Proxy settings are incomplete."));
        return;
    }

    auto normalizedServerUrl = normalizeServerUrlInput(_serverUrl);
    const auto url = QUrl(normalizedServerUrl, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty()) {
        setErrorText(tr("Server address does not seem to be valid"));
        return;
    }

    ensureAccount();
    setPublicShareSetup(false);
    setBasicAuthUser({});
    setBasicAuthPassword({});
    normalizedServerUrl = normalizeServerUrlInput(_serverUrl, _account->davPath());
    const auto accountUrl = QUrl(normalizedServerUrl, QUrl::StrictMode);
    setServerUrl(accountUrl.toString());
    startServerCheck(accountUrl);
}

void AccountWizardController::submitBasicAuth()
{
    if (_busy || !_account) {
        return;
    }

    if (!basicAuthValid()) {
        setErrorText(tr("Username must not be empty."));
        return;
    }

    setBusy(true);
    setErrorText({});
    setAuthStatusText(tr("Checking account access") + QStringLiteral("…"));

    auto *credentials = new HttpCredentialsGui(_basicAuthUser, _basicAuthPassword, _clientCertBundle, _clientCertPassword);
    _account->setCredentials(credentials);
    credentials->persist();
    _account->clearCookieJar();

    if (_account->isPublicShareLink()) {
        _account->setDavUser(_basicAuthUser);
        _account->setDavDisplayName(_basicAuthUser);
        setUserDisplayName(_account->prettyName());
        setServerDisplayName(_account->url().host());
        testOwnCloudConnect();
        return;
    }

    auto *fetchUserNameJob = new JsonApiJob(_account, QStringLiteral("/ocs/v1.php/cloud/user"), this);
    connect(fetchUserNameJob, &JsonApiJob::jsonReceived, this, [this, fetchUserNameJob](const QJsonDocument &json, int statusCode) {
        if (statusCode != 100) {
            qCWarning(lcAccountWizardController) << "Could not fetch username.";
        }

        fetchUserNameJob->deleteLater();

        const auto objData = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject();
        const auto userId = objData.value("id"_L1).toString(QString());
        const auto displayName = objData.value("display-name"_L1).toString(QString());
        _account->setDavUser(userId);
        _account->setDavDisplayName(displayName);
        setUserDisplayName(displayName.isEmpty() ? userId : displayName);
        setAvatarUrl({});
        fetchUserAvatar();

        testOwnCloudConnect();
    });
    fetchUserNameJob->start();
}

void AccountWizardController::startServerCheck(const QUrl &serverUrl)
{
    _account->setUrl(serverUrl);
    _account->setCredentials(CredentialsFactory::create("dummy"));
    const auto proxyType = proxySettingsAvailable() ? _proxySettings._proxyType : QNetworkProxy::NoProxy;
    _account->setProxyType(proxyType);

    switch (proxyType) {
    case QNetworkProxy::HttpCachingProxy:
    case QNetworkProxy::FtpCachingProxy:
    case QNetworkProxy::NoProxy:
    case QNetworkProxy::DefaultProxy:
        _account->networkAccessManager()->setProxy({QNetworkProxy::NoProxy});
        break;
    case QNetworkProxy::Socks5Proxy:
    case QNetworkProxy::HttpProxy:
        _account->setProxyHostName(_proxySettings._host);
        _account->setProxyPort(_proxySettings._port);
        _account->setProxyNeedsAuth(_proxySettings._needsAuth == ProxyAuthentication::AuthenticationRequired);
        if (_account->proxyNeedsAuth()) {
            _account->setProxyUser(_proxySettings._user);
            _account->setProxyPassword(_proxySettings._password);
        }
        break;
    }

    _account->setSslConfiguration(QSslConfiguration::defaultConfiguration());
    auto sslConfiguration = _account->getOrCreateSslConfig();
    if (!_clientSslCertificate.isNull()) {
        sslConfiguration.setLocalCertificate(_clientSslCertificate);
        sslConfiguration.setPrivateKey(_clientSslKey);
        auto caCertificates = sslConfiguration.systemCaCertificates();
        caCertificates.append(_clientSslCaCertificates);
        sslConfiguration.setCaCertificates(caCertificates);
    }
    _account->setSslConfiguration(sslConfiguration);
    _account->networkAccessManager()->clearAccessCache();

    setErrorText({});
    setBusy(true);
    setAuthStatusText(tr("Checking server address") + QStringLiteral("…"));

    if (proxySettingsAvailable() && (ClientProxy::isUsingSystemDefault() || _account->proxyType() == QNetworkProxy::DefaultProxy)) {
        ClientProxy::lookupSystemProxyAsync(_account->url(), this, SLOT(slotSystemProxyLookupDone(QNetworkProxy)));
    } else {
        _account->networkAccessManager()->setProxy(QNetworkProxy(proxySettingsAvailable() ? QNetworkProxy::DefaultProxy : QNetworkProxy::NoProxy));
        QMetaObject::invokeMethod(this, "slotFindServer", Qt::QueuedConnection);
    }
}

void AccountWizardController::slotSystemProxyLookupDone(const QNetworkProxy &proxy)
{
    _account->networkAccessManager()->setProxy(proxy);
    slotFindServer();
}

void AccountWizardController::slotFindServer()
{
    auto *job = new CheckServerJob(_account, this);
    job->setIgnoreCredentialFailure(true);
    connect(job, &CheckServerJob::instanceFound, this, &AccountWizardController::slotFoundServer);
    connect(job, &CheckServerJob::instanceNotFound, this, &AccountWizardController::slotFindServerBehindRedirect);
    connect(job, &CheckServerJob::timeout, this, &AccountWizardController::slotNoServerFoundTimeout);
    job->setTimeout((_account->url().scheme() == "https"_L1) ? 30 * 1000 : 10 * 1000);
    job->start();
}

void AccountWizardController::slotFindServerBehindRedirect()
{
    auto redirectCheckJob = _account->sendRequest("GET", _account->url());
    redirectCheckJob->setTimeout(qMin(2000ll, redirectCheckJob->timeoutMsec()));

    auto permanentRedirects = std::make_shared<int>(0);
    connect(redirectCheckJob, &AbstractNetworkJob::redirected, this,
        [permanentRedirects, account = _account](QNetworkReply *reply, const QUrl &targetUrl, int count) {
            const auto httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (count == *permanentRedirects && (httpCode == 301 || httpCode == 308)) {
                qCInfo(lcAccountWizardController) << account->url() << "was redirected to" << targetUrl;
                account->setUrl(targetUrl);
                *permanentRedirects += 1;
            }
        });

    connect(redirectCheckJob, &SimpleNetworkJob::finishedSignal, this, [this] {
        auto *job = new CheckServerJob(_account, this);
        job->setIgnoreCredentialFailure(true);
        connect(job, &CheckServerJob::instanceFound, this, &AccountWizardController::slotFoundServer);
        connect(job, &CheckServerJob::instanceNotFound, this, &AccountWizardController::slotNoServerFound);
        connect(job, &CheckServerJob::timeout, this, &AccountWizardController::slotNoServerFoundTimeout);
        job->setTimeout((_account->url().scheme() == "https"_L1) ? 30 * 1000 : 10 * 1000);
        job->start();
    });
}

void AccountWizardController::slotFoundServer(const QUrl &url, const QJsonObject &info)
{
    const auto serverVersion = CheckServerJob::version(info);
    _account->setServerVersion(serverVersion);

    if (url != _account->url()) {
        _account->setUrl(url);
        setServerUrl(url.toString());
    }

    setServerDisplayName(url.host());
    setAuthStatusText(tr("Preparing browser login") + QStringLiteral("…"));

    if (_account->isPublicShareLink()) {
        setPublicShareSetup(true);
        setBasicAuthUser(_account->davUser());
        setBasicAuthPassword({});
        setBusy(false);
        setAuthStatusText({});
        setCurrentStep(BasicAuthStep);
        QTimer::singleShot(0, this, &AccountWizardController::submitBasicAuth);
        return;
    }

    slotDetermineAuthType();
}

void AccountWizardController::slotNoServerFound(QNetworkReply *reply)
{
    const auto job = qobject_cast<CheckServerJob *>(sender());
    QString message;
    if (!_account->url().isValid()) {
        message = tr("Invalid URL");
    } else {
        message = tr("Failed to connect to %1 at %2:\n%3")
            .arg(Theme::instance()->appNameGUI(),
                 _account->url().toString(),
                 job ? job->errorString() : QString{});
    }

    setBusy(false);
    setErrorText(message);
    _account->resetRejectedCertificates();

    static_cast<void>(handleSecureConnectionFailure(reply, checkDowngradeAdvised(reply)));
}

void AccountWizardController::slotNoServerFoundTimeout(const QUrl &url)
{
    setBusy(false);
    setErrorText(tr("Timeout while trying to connect to %1 at %2.")
        .arg(Utility::escape(Theme::instance()->appNameGUI()), Utility::escape(url.toString())));
    static_cast<void>(handleSecureConnectionFailure(nullptr, false));
}

void AccountWizardController::slotDetermineAuthType()
{
    auto *job = new DetermineAuthTypeJob(_account, this);
    connect(job, &DetermineAuthTypeJob::authType, this, [this](DetermineAuthTypeJob::AuthType type) {
        switch (type) {
        case DetermineAuthTypeJob::LoginFlowV2:
            startFlow2Auth();
            break;
#ifdef WITH_WEBENGINE
        case DetermineAuthTypeJob::WebViewFlow:
            if (ConfigFile().forceLoginV2()) {
                startFlow2Auth();
                break;
            }
            setBusy(false);
            setAuthStatusText({});
            setErrorText(tr("This server requires legacy browser authentication. Enter app-password credentials instead."));
            setCurrentStep(BasicAuthStep);
            break;
#endif
        case DetermineAuthTypeJob::Basic:
        case DetermineAuthTypeJob::NoAuthType:
            setBusy(false);
            setAuthStatusText({});
            setErrorText({});
            setCurrentStep(BasicAuthStep);
            break;
        }
    });
    job->start();
}

void AccountWizardController::startFlow2Auth()
{
    _account->setCredentials(CredentialsFactory::create("http"));

    discardFlow2Auth();

    _flow2Auth = std::make_unique<Flow2Auth>(_account.data(), this);
    connect(_flow2Auth.get(), &Flow2Auth::result, this, &AccountWizardController::slotFlow2AuthResult, Qt::QueuedConnection);
    connect(_flow2Auth.get(), &Flow2Auth::statusChanged, this, &AccountWizardController::slotFlow2StatusChanged);

    setBusy(false);
    setErrorText({});
    setCurrentStep(BrowserAuthStep);
    _flow2Auth->start();
}

void AccountWizardController::openBrowserLogin()
{
    if (_flow2Auth) {
        setErrorText({});
        _flow2Auth->openBrowser();
    }
}

void AccountWizardController::copyLoginLink()
{
    if (_flow2Auth) {
        setErrorText({});
        _flow2Auth->copyLinkToClipboard();
    }
}

void AccountWizardController::openSignup()
{
    Utility::openBrowser(QUrl(QStringLiteral("https://nextcloud.com/register")));
}

void AccountWizardController::openSelfHostedServerGuide()
{
    Utility::openBrowser(QUrl(QStringLiteral("https://docs.nextcloud.com/server/latest/admin_manual/installation/#installation")));
}

void AccountWizardController::openProxySettings()
{
    if (!proxySettingsAvailable()) {
        return;
    }

    emit proxySettingsRequested();
}

void AccountWizardController::pollNow()
{
    if (_flow2Auth) {
        _flow2Auth->slotPollNow();
    }
}

void AccountWizardController::slotFlow2AuthResult(Flow2Auth::Result result, const QString &errorString, const QString &user, const QString &appPassword)
{
    switch (result) {
    case Flow2Auth::NotSupported:
        setErrorText(tr("Unable to open the Browser, please copy the link to your Browser."));
        break;
    case Flow2Auth::Error:
        setAuthPolling(false);
        setBusy(false);
        setErrorText(errorString);
        break;
    case Flow2Auth::LoggedIn:
        setAuthPolling(false);
        connectToAuthenticatedAccount(_account->url().toString(), user, appPassword);
        break;
    }
}

void AccountWizardController::slotFlow2StatusChanged(Flow2Auth::PollStatus status, int secondsLeft)
{
    if (_flow2Auth) {
        setLoginUrl(_flow2Auth->authorisationLink());
    }

    switch (status) {
    case Flow2Auth::statusPollCountdown:
        Q_UNUSED(secondsLeft)
        setAuthPolling(true);
        setBusy(false);
        setAuthStatusText(tr("Waiting for authorization") + QStringLiteral("…"));
        break;
    case Flow2Auth::statusPollNow:
        setAuthPolling(true);
        setBusy(false);
        setAuthStatusText(tr("Waiting for authorization") + QStringLiteral("…"));
        break;
    case Flow2Auth::statusFetchToken:
        setAuthPolling(false);
        setBusy(true);
        setAuthStatusText(tr("Starting authorization") + QStringLiteral("…"));
        break;
    case Flow2Auth::statusCopyLinkToClipboard:
        setAuthPolling(true);
        setBusy(false);
        setAuthStatusText(tr("Link copied to clipboard."));
        break;
    }
}

void AccountWizardController::connectToAuthenticatedAccount(const QString &url, const QString &user, const QString &appPassword)
{
    setBusy(true);
    setErrorText({});
    setAuthStatusText(tr("Checking account access") + QStringLiteral("…"));

    auto *credentials = new WebFlowCredentials(user, appPassword, _clientSslCertificate, _clientSslKey, _clientSslCaCertificates);
    _account->setCredentials(credentials);
    credentials->persist();

    auto *fetchUserNameJob = new JsonApiJob(_account, QStringLiteral("/ocs/v1.php/cloud/user"), this);
    connect(fetchUserNameJob, &JsonApiJob::jsonReceived, this, [this, fetchUserNameJob, url](const QJsonDocument &json, int statusCode) {
        if (statusCode != 100) {
            qCWarning(lcAccountWizardController) << "Could not fetch username.";
        }

        fetchUserNameJob->deleteLater();

        const auto objData = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject();
        const auto userId = objData.value("id"_L1).toString(QString());
        const auto displayName = objData.value("display-name"_L1).toString(QString());
        _account->setDavUser(userId);
        _account->setDavDisplayName(displayName);
        setUserDisplayName(displayName.isEmpty() ? userId : displayName);
        setServerUrl(url);
        setAvatarUrl({});
        fetchUserAvatar();

        testOwnCloudConnect();
    });
    fetchUserNameJob->start();
}

void AccountWizardController::testOwnCloudConnect()
{
    auto *job = new PropfindJob(_account, "/", this);
    job->setIgnoreCredentialFailure(true);
    job->setFollowRedirects(false);
    job->setProperties(QList<QByteArray>() << "getlastmodified");
    connect(job, &PropfindJob::result, this, &AccountWizardController::completeAuthentication);
    connect(job, &PropfindJob::finishedWithError, this, &AccountWizardController::slotAuthError);
    job->start();
}

void AccountWizardController::slotAuthError(QNetworkReply *reply)
{
    QString errorMessage;

    if (!reply) {
        setBusy(false);
        setErrorText(tr("There was an invalid response to an authenticated WebDAV request"));
        return;
    }

    const auto redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (!redirectUrl.isEmpty()) {
        auto adjustedRedirectUrl = redirectUrl;
        auto path = adjustedRedirectUrl.path();
        const auto expectedPath = QString{u'/' + _account->davPath()};
        if (path.endsWith(expectedPath)) {
            path.chop(expectedPath.size());
            adjustedRedirectUrl.setPath(path);
            _account->setUrl(adjustedRedirectUrl);
            testOwnCloudConnect();
            return;
        }

        errorMessage = tr("The authenticated request to the server was redirected to \"%1\". The URL is bad, the server is misconfigured.")
            .arg(Utility::escape(redirectUrl.toString()));
    } else if (reply->error() == QNetworkReply::ContentNotFoundError) {
        completeAuthentication();
        return;
    } else if (reply->error() != QNetworkReply::NoError) {
        const auto job = qobject_cast<PropfindJob *>(sender());
        if (!_account->credentials()->stillValid(reply)) {
            errorMessage = tr("Access forbidden by server. To verify that you have proper access, open the service in your browser.");
        } else if (job) {
            errorMessage = job->errorStringParsingBody();
        }
    } else {
        errorMessage = tr("There was an invalid response to an authenticated WebDAV request");
    }

    setBusy(false);
    if (_currentStep != BasicAuthStep) {
        setCurrentStep(BrowserAuthStep);
    }
    setErrorText(errorMessage);
}

void AccountWizardController::completeAuthentication()
{
    setBusy(false);
    setAuthStatusText(tr("Account connected."));
    initialiseLocalSyncFolder();
    fetchRootFolderSize();

#ifdef BUILD_FILE_PROVIDER_MODULE
    setNeedsSyncOptions(!canUseVirtualFiles());
#else
    setNeedsSyncOptions(true);
#endif

    if (needsSyncOptions()) {
        ConfigFile cfg;
        if (Theme::instance()->forceOverrideServerUrl() && !cfg.overrideLocalDir().isEmpty()) {
            QTimer::singleShot(0, this, &AccountWizardController::finish);
            return;
        }

        setCurrentStep(SyncOptionsStep);
        if (Theme::instance()->wizardSelectiveSyncDefaultNothing() && canUseClassicSync()) {
            _selectiveSyncBlacklist = QStringList(QStringLiteral("/"));
            QTimer::singleShot(0, this, &AccountWizardController::openSelectiveSync);
        }
    } else {
        finish();
    }
}

void AccountWizardController::fetchUserAvatar()
{
    if (!_account || _account->davUser().isEmpty() || _account->isPublicShareLink()) {
        return;
    }

    auto avatarSize = 80;
    if (Theme::isHidpi()) {
        avatarSize *= 2;
    }

    const auto avatarJob = new AvatarJob(_account, _account->davUser(), avatarSize, this);
    avatarJob->setTimeout(20 * 1000);
    connect(avatarJob, &AvatarJob::avatarPixmap, this, [this](const QImage &avatarImage) {
        if (avatarImage.isNull()) {
            return;
        }

        _account->setAvatar(avatarImage);

        QByteArray pngData;
        QBuffer pngBuffer(&pngData);
        pngBuffer.open(QIODevice::WriteOnly);
        AvatarJob::makeCircularAvatar(avatarImage).save(&pngBuffer, "PNG");
        setAvatarUrl(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(pngData.toBase64()));
    });
    avatarJob->start();
}

void AccountWizardController::fetchRootFolderSize()
{
    if (!_account) {
        return;
    }

    const auto quotaJob = new PropfindJob(_account, _remoteFolder, this);
    quotaJob->setProperties(QList<QByteArray>() << "http://owncloud.org/ns:size");

    connect(quotaJob, &PropfindJob::result, this, [this](const QVariantMap &result) {
        bool ok = false;
        auto size = result.value("size"_L1).toLongLong(&ok);
        if (!ok) {
            const auto floatingPointSize = result.value("size"_L1).toDouble(&ok);
            size = ok ? static_cast<qint64>(floatingPointSize) : -1;
        }

        if (size >= 0) {
            _syncEverythingSize = size;
            setSyncEverythingDescription(tr("Will require %1 of storage").arg(Utility::octetsToString(size)));
            validateLocalSyncFolder();
        }
    });
    connect(quotaJob, &PropfindJob::finishedWithError, this, [this](QNetworkReply *) {
        _syncEverythingSize = -1;
        setSyncEverythingDescription({});
        validateLocalSyncFolder();
    });
    quotaJob->start();
}

void AccountWizardController::cancel()
{
    emit finished(QDialog::Rejected);
}

void AccountWizardController::goBack()
{
    setErrorText({});
    if (_currentStep == BrowserAuthStep || _currentStep == BasicAuthStep || _currentStep == SyncOptionsStep) {
        discardFlow2Auth();
        setPublicShareSetup(false);
        setCurrentStep(ServerStep);
        setBusy(false);
        setAuthStatusText({});
    }
}

void AccountWizardController::finish()
{
    if (_busy) {
        return;
    }

    if (!_account) {
        emit finished(QDialog::Rejected);
        return;
    }

    if (localSyncFolderRequired()) {
        validateLocalSyncFolder();
        if (!_localSyncFolderValid || !ensureLocalSyncFolder()) {
            return;
        }
    }

    if (_syncMode == SyncEverything) {
        ConfigFile cfgFile;
        cfgFile.setNewBigFolderSizeLimit(_askBeforeLargeFolders, _largeFolderThresholdMb);
        cfgFile.setConfirmExternalStorage(_askBeforeExternalStorage);
    }

    if (localSyncFolderRequired()) {
        startRemoteFolderCheck();
        return;
    }

    completeRemoteFolderCheck();
}

void AccountWizardController::skipFolderConfiguration()
{
    if (!_account) {
        emit finished(QDialog::Rejected);
        return;
    }

    applyAccountChanges();
    _account = AccountManager::createAccount();
    clearOneShotOverrides();
    setCurrentStep(CompletedStep);
    emit finished(QDialog::Accepted);
}

AccountState *AccountWizardController::applyAccountChanges()
{
    auto manager = AccountManager::instance();
    AccountState *accountState = nullptr;

#ifdef BUILD_FILE_PROVIDER_MODULE
    if (_syncMode == VirtualFiles) {
        accountState = manager->addAccount(_account);
        const auto accountId = accountState->account()->userIdAtHostWithPort();
        Mac::FileProviderSettingsController::instance()->setVfsEnabledForAccount(accountId, true, false);
    } else
#endif
    {
        accountState = manager->addAccount(_account);
    }

    manager->saveAccount(_account);
    return accountState;
}

void AccountWizardController::clearOneShotOverrides()
{
    ConfigFile cfg;
    cfg.setOverrideServerUrl({});
    cfg.setOverrideLocalDir({});
}

void AccountWizardController::initialiseLocalSyncFolder()
{
    ConfigFile cfg;
    const auto overrideLocalDir = !cfg.overrideLocalDir().isEmpty();
    _localSyncFolderOverride = overrideLocalDir;
    QString localFolder;
    if (overrideLocalDir) {
        localFolder = cfg.overrideLocalDir();
    } else {
#ifndef Q_OS_MACOS
        localFolder = Theme::instance()->defaultClientFolder();
        if (!QDir(localFolder).isAbsolute()) {
            localFolder = QDir::homePath() + QLatin1Char('/') + localFolder;
        }
#endif
    }

    const auto strategy = overrideLocalDir
        ? FolderMan::GoodPathStrategy::AllowOverrideExistingPath
        : FolderMan::GoodPathStrategy::AllowOnlyNewPath;
    setLocalSyncFolder(localFolder.isEmpty()
        ? QString{}
        : FolderMan::instance()->findGoodPathForNewSyncFolder(localFolder, localFolderServerUrl(), strategy),
        overrideLocalDir);
}

void AccountWizardController::setLocalSyncFolder(const QString &localSyncFolder, bool selectedByUser)
{
    const auto normalizedLocalSyncFolder = QDir::fromNativeSeparators(localSyncFolder);
    const auto localSyncFolderSelected = _localSyncFolderSelected || selectedByUser;
    if (_localSyncFolder == normalizedLocalSyncFolder && _localSyncFolderSelected == localSyncFolderSelected) {
        validateLocalSyncFolder();
        return;
    }

    _localSyncFolder = normalizedLocalSyncFolder;
    _localSyncFolderSelected = localSyncFolderSelected;
    emit localSyncFolderChanged();
    validateLocalSyncFolder();
}

void AccountWizardController::promptForInitialLocalSyncFolderIfNeeded()
{
#ifdef Q_OS_MACOS
    if (_initialLocalSyncFolderPromptShown
        || _localSyncFolderSelected
        || _currentStep != SyncOptionsStep
        || !localSyncFolderRequired()) {
        return;
    }

    _initialLocalSyncFolderPromptShown = true;
    QTimer::singleShot(0, this, [this] {
        if (_currentStep != SyncOptionsStep || _localSyncFolderSelected || !localSyncFolderRequired()) {
            return;
        }

        _localSyncFolderPickerOpen = true;
        validateLocalSyncFolder();
        const auto selectedFolder = openLocalSyncFolderDialog(true);
        _localSyncFolderPickerOpen = false;
        if (!selectedFolder.isEmpty()) {
            setLocalSyncFolder(selectedFolder, true);
        } else {
            validateLocalSyncFolder();
        }
    });
#endif
}

void AccountWizardController::validateLocalSyncFolder()
{
    const auto oldCanFinish = canFinish();
    const auto folderRequired = localSyncFolderRequired();
    auto localSyncFolderFreeSpace = QString{};

    if (folderRequired) {
        const auto freeBytes = availableLocalSpace();
        if (freeBytes >= 0) {
            localSyncFolderFreeSpace = tr("%1 free space", "%1 gets replaced with the size and a matching unit. Example: 3 MB or 5 GB")
                .arg(Utility::octetsToString(freeBytes));
        }
    }

    auto localSyncFolderError = FolderMan::instance()->checkPathValidityForNewFolder(_localSyncFolder, localFolderServerUrl()).second;

    const auto neededBytes = requiredLocalSpace();
    const auto freeBytes = availableLocalSpace();
    if (folderRequired && localSyncFolderError.isEmpty() && neededBytes >= 0 && freeBytes >= 0 && freeBytes <= neededBytes) {
        localSyncFolderError = tr("There isn't enough free space in the local folder!");
    }

#ifndef BUILD_FILE_PROVIDER_MODULE
    if (folderRequired && _syncMode == VirtualFiles && localSyncFolderError.isEmpty()) {
        const auto availability = Vfs::checkAvailability(FolderDefinition::prepareLocalPath(_localSyncFolder), bestAvailableVfsMode());
        if (!availability) {
            localSyncFolderError = availability.error();
        }
    }
#endif

#ifdef Q_OS_MACOS
    if (folderRequired && _localSyncFolderPickerOpen) {
        localSyncFolderError.clear();
    }
    if (folderRequired && !_localSyncFolderSelected && !_localSyncFolderPickerOpen) {
        localSyncFolderError = tr("Please choose a local sync folder.");
    }
#endif
    if (folderRequired && _localSyncFolderSelected && !_localSyncFolderOverride && localSyncFolderError.isEmpty()) {
        if (localFolderContainsData(_localSyncFolder)) {
            localSyncFolderError = tr("Please choose an empty local sync folder.");
        }
    }
    const auto localSyncFolderValid = !folderRequired
        || (!_localSyncFolderPickerOpen && localSyncFolderError.isEmpty());

    if (_localSyncFolderFreeSpace != localSyncFolderFreeSpace) {
        _localSyncFolderFreeSpace = localSyncFolderFreeSpace;
        emit localSyncFolderFreeSpaceChanged();
    }

    if (_localSyncFolderError != localSyncFolderError) {
        _localSyncFolderError = localSyncFolderError;
        emit localSyncFolderErrorChanged();
    }

    if (_localSyncFolderValid != localSyncFolderValid) {
        _localSyncFolderValid = localSyncFolderValid;
        if (oldCanFinish != canFinish()) {
            emit canFinishChanged();
        }
    }
}

qint64 AccountWizardController::availableLocalSpace() const
{
    if (_localSyncFolder.isEmpty()) {
        return -1;
    }

    const auto localFolder = FolderDefinition::prepareLocalPath(_localSyncFolder);
    const auto homePath =
#ifdef Q_OS_MACOS
        Utility::getRealHomeDirectory();
#else
        QDir::homePath();
#endif
    const auto path = !QDir(localFolder).exists() && localFolder.contains(homePath)
        ? homePath
        : localFolder;
    const QStorageInfo storage(QDir::toNativeSeparators(path));
    return storage.isValid() ? storage.bytesAvailable() : -1;
}

qint64 AccountWizardController::requiredLocalSpace() const
{
    switch (_syncMode) {
    case SyncEverything:
        return _syncEverythingSize;
    case SelectiveSync:
        return _selectiveSyncSize;
    case VirtualFiles:
        return -1;
    }

    return -1;
}

bool AccountWizardController::ensureLocalSyncFolder()
{
    const auto localFolder = FolderDefinition::prepareLocalPath(_localSyncFolder);
    QDir dir(localFolder);
    if (!dir.exists()) {
        qCInfo(lcAccountWizardController) << "Creating local sync folder" << localFolder;
        if (!dir.mkpath(".")) {
            setErrorText(tr("Could not create local folder %1").arg(Utility::escape(QDir::toNativeSeparators(localFolder))));
            return false;
        }
    }

    FileSystem::setFolderMinimumPermissions(localFolder);
    Utility::setupFavLink(localFolder);
    return true;
}

void AccountWizardController::startRemoteFolderCheck()
{
    setBusy(true);
    setErrorText({});
    setAuthStatusText(tr("Checking remote folder") + QStringLiteral("…"));

    auto *job = new EntityExistsJob(_account, sanitizedRemoteFolderEntityPath(), this);
    connect(job, &EntityExistsJob::exists, this, &AccountWizardController::slotRemoteFolderExists);
    job->start();
}

QString AccountWizardController::sanitizedRemoteFolderEntityPath() const
{
    if (!_account) {
        return {};
    }

    auto davPath = _account->davPath();
    auto remoteFolder = _remoteFolder;

    while (davPath.startsWith('/'_L1)) {
        davPath.remove(0, 1);
    }
    while (davPath.endsWith('/'_L1)) {
        davPath.chop(1);
    }

    while (remoteFolder.startsWith('/'_L1)) {
        remoteFolder.remove(0, 1);
    }
    while (remoteFolder.endsWith('/'_L1)) {
        remoteFolder.chop(1);
    }

    return davPath + QLatin1Char('/') + remoteFolder;
}

void AccountWizardController::slotRemoteFolderExists(QNetworkReply *reply)
{
    const auto job = qobject_cast<EntityExistsJob *>(sender());
    const auto error = reply ? reply->error() : QNetworkReply::UnknownNetworkError;

    if (error == QNetworkReply::NoError) {
        completeRemoteFolderCheck();
        return;
    }

    if (error == QNetworkReply::ContentNotFoundError) {
        if (_remoteFolder.isEmpty()) {
            setBusy(false);
            setErrorText(tr("No remote folder specified!"));
            return;
        }

        createRemoteFolder();
        return;
    }

    setBusy(false);
    setErrorText(tr("Error: %1").arg(job ? job->errorString() : QString{}));
}

void AccountWizardController::createRemoteFolder()
{
    setAuthStatusText(tr("Creating remote folder") + QStringLiteral("…"));

    auto *job = new MkColJob(_account, _remoteFolder, this);
    connect(job, &MkColJob::finishedWithError, this, &AccountWizardController::slotCreateRemoteFolderFinished);
    connect(job, &MkColJob::finishedWithoutError, this, &AccountWizardController::completeRemoteFolderCheck);
    job->start();
}

void AccountWizardController::slotCreateRemoteFolderFinished(QNetworkReply *reply)
{
    const auto networkError = reply ? reply->error() : QNetworkReply::UnknownNetworkError;
    const auto httpStatusCode = reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;
    if (httpStatusCode >= 200 && httpStatusCode < 300) {
        completeRemoteFolderCheck();
        return;
    }

    setBusy(false);
    if (httpStatusCode > 0) {
        setErrorText(tr("The folder creation resulted in HTTP error code %1").arg(httpStatusCode));
    } else if (networkError == QNetworkReply::OperationCanceledError) {
        setErrorText(tr("The remote folder creation failed because the provided credentials are wrong. Please go back and check your credentials."));
    } else {
        setErrorText(tr("Remote folder %1 creation failed with error <tt>%2</tt>.")
            .arg(Utility::escape(_remoteFolder), QString::number(static_cast<int>(networkError))));
    }
}

void AccountWizardController::completeRemoteFolderCheck()
{
    const auto accountState = applyAccountChanges();
    if (localSyncFolderRequired() && !createSyncFolder(accountState)) {
        AccountManager::instance()->removeAccountState(accountState);
        setBusy(false);
        return;
    }

    _account = AccountManager::createAccount();
    setBusy(false);
    clearOneShotOverrides();
    setCurrentStep(CompletedStep);
    emit finished(QDialog::Accepted);
}

bool AccountWizardController::createSyncFolder(AccountState *accountState)
{
    if (!accountState) {
        setErrorText(tr("Account setup failed while creating the sync folder."));
        return false;
    }

    FolderDefinition folderDefinition;
    folderDefinition.localPath = FolderDefinition::prepareLocalPath(_localSyncFolder);
    folderDefinition.targetPath = FolderDefinition::prepareTargetPath(_remoteFolder);
    folderDefinition.ignoreHiddenFiles = FolderMan::instance()->ignoreHiddenFiles();

#ifndef BUILD_FILE_PROVIDER_MODULE
    if (_syncMode == VirtualFiles) {
        folderDefinition.virtualFilesMode = bestAvailableVfsMode();
    }
#endif

#ifdef Q_OS_WIN
    if (FolderMan::instance()->navigationPaneHelper().showInExplorerNavigationPane()) {
        folderDefinition.navigationPaneClsid = QUuid::createUuid();
    }
#endif

    auto *folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(false);
    const auto folder = folderMan->addFolder(accountState, folderDefinition);
    folderMan->setSyncEnabled(true);

    if (!folder) {
        setErrorText(tr("Could not create the sync folder."));
        return false;
    }

    if (folderDefinition.virtualFilesMode != Vfs::Off && _syncMode == VirtualFiles) {
        folder->setRootPinState(PinState::OnlineOnly);
    }

    folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, _selectiveSyncBlacklist);
    const auto preWhitelistRoot = _syncMode != SyncEverything || (!_askBeforeLargeFolders && !_askBeforeExternalStorage);
    folder->journalDb()->setSelectiveSyncList(
        SyncJournalDb::SelectiveSyncWhiteList,
        preWhitelistRoot ? QStringList() << QLatin1String("/") : QStringList());
    folderMan->scheduleAllFolders();
    return true;
}

QUrl AccountWizardController::localFolderServerUrl() const
{
    if (!_account) {
        return {};
    }

    auto url = _account->url();
    url.setUserName(_account->credentials() ? _account->credentials()->user() : QString{});
    return url;
}

void AccountWizardController::setSyncMode(int syncMode)
{
    if (syncMode < SyncEverything || syncMode > VirtualFiles) {
        return;
    }

    const auto newSyncMode = static_cast<SyncMode>(syncMode);
    if (_syncMode == newSyncMode) {
        return;
    }

    if (newSyncMode == VirtualFiles && !canUseVirtualFiles()) {
        return;
    }
    if (newSyncMode != VirtualFiles && !canUseClassicSync()) {
        return;
    }

    const auto oldLocalSyncFolderRequired = localSyncFolderRequired();
    const auto oldCanFinish = canFinish();
    _syncMode = newSyncMode;
    emit syncModeChanged();

    if (oldLocalSyncFolderRequired != localSyncFolderRequired()) {
        emit localSyncFolderRequiredChanged();
    }
    validateLocalSyncFolder();
    if (oldCanFinish != canFinish()) {
        emit canFinishChanged();
    }
    promptForInitialLocalSyncFolderIfNeeded();
}

void AccountWizardController::chooseLocalSyncFolder()
{
    const auto selectedFolder = openLocalSyncFolderDialog(false);
    if (selectedFolder.isEmpty()) {
        return;
    }

    _localSyncFolderOverride = false;
    setLocalSyncFolder(selectedFolder, true);
}

QString AccountWizardController::openLocalSyncFolderDialog(bool initialSelection) const
{
    QString startFolder = _localSyncFolder;
    if (initialSelection) {
#ifdef Q_OS_MACOS
        startFolder = Utility::getRealHomeDirectory();
#else
        startFolder = QDir::homePath();
#endif
    } else if (startFolder.isEmpty()) {
#ifdef Q_OS_MACOS
        startFolder = Utility::getRealHomeDirectory();
#else
        startFolder = QDir::homePath();
#endif
    }

    return QFileDialog::getExistingDirectory(nullptr,
        tr("Local Sync Folder"),
        startFolder,
        QFileDialog::ShowDirsOnly);
}

void AccountWizardController::openSelectiveSync()
{
    if (!_account || !canUseClassicSync()) {
        return;
    }

    const auto previousSyncMode = _syncMode;
    const auto previousBlacklist = _selectiveSyncBlacklist;
    const auto previousSelectiveSyncSize = _selectiveSyncSize;
    setSyncMode(SelectiveSync);
    if (!_selectiveSyncDialog) {
        _selectiveSyncDialog = new SelectiveSyncDialog(_account,
            _remoteFolder,
            _selectiveSyncBlacklist,
            nullptr,
            Qt::Dialog | Qt::FramelessWindowHint);
        _selectiveSyncDialog->setAttribute(Qt::WA_DeleteOnClose);
        _selectiveSyncDialog->setWindowModality(Qt::ApplicationModal);
        connect(_selectiveSyncDialog, &SelectiveSyncDialog::finished, this, [this, previousSyncMode, previousBlacklist, previousSelectiveSyncSize] {
            if (!_selectiveSyncDialog) {
                return;
            }
            if (_selectiveSyncDialog->result() == QDialog::Accepted) {
                _selectiveSyncBlacklist = _selectiveSyncDialog->createBlackList();
                _selectiveSyncSize = _selectiveSyncDialog->estimatedSize();
                if (_selectiveSyncBlacklist.isEmpty()) {
                    setSyncMode(SyncEverything);
                } else {
                    setSyncMode(SelectiveSync);
                }
            } else {
                _selectiveSyncBlacklist = previousBlacklist;
                _selectiveSyncSize = previousSelectiveSyncSize;
                setSyncMode(previousSyncMode);
            }
            validateLocalSyncFolder();
        });
    }
    _selectiveSyncDialog->open();
}

void AccountWizardController::openAdvancedOptions()
{
    if (hasAdvancedOptions()) {
        emit advancedOptionsRequested();
    }
}

void AccountWizardController::setAskBeforeLargeFolders(bool ask)
{
    if (_askBeforeLargeFolders == ask) {
        return;
    }
    _askBeforeLargeFolders = ask;
    emit askBeforeLargeFoldersChanged();
}

void AccountWizardController::setLargeFolderThresholdMb(int thresholdMb)
{
    const auto boundedThreshold = qMax(0, thresholdMb);
    if (_largeFolderThresholdMb == boundedThreshold) {
        return;
    }
    _largeFolderThresholdMb = boundedThreshold;
    emit largeFolderThresholdMbChanged();
}

void AccountWizardController::setAskBeforeExternalStorage(bool ask)
{
    if (_askBeforeExternalStorage == ask) {
        return;
    }
    _askBeforeExternalStorage = ask;
    emit askBeforeExternalStorageChanged();
}

void AccountWizardController::setCurrentStep(Step step)
{
    if (_currentStep == step) {
        return;
    }
    _currentStep = step;
    emit currentStepChanged();
    promptForInitialLocalSyncFolderIfNeeded();
}

void AccountWizardController::setBusy(bool busy)
{
    if (_busy == busy) {
        return;
    }
    _busy = busy;
    emit busyChanged();
}

void AccountWizardController::setAuthPolling(bool authPolling)
{
    if (_authPolling == authPolling) {
        return;
    }
    _authPolling = authPolling;
    emit authPollingChanged();
}

void AccountWizardController::setErrorText(const QString &errorText)
{
    if (_errorText == errorText) {
        return;
    }
    _errorText = errorText;
    emit errorTextChanged();
}

void AccountWizardController::setLoginUrl(const QUrl &loginUrl)
{
    if (_loginUrl == loginUrl) {
        return;
    }
    _loginUrl = loginUrl;
    emit loginUrlChanged();
}

void AccountWizardController::setAuthStatusText(const QString &authStatusText)
{
    if (_authStatusText == authStatusText) {
        return;
    }
    _authStatusText = authStatusText;
    emit authStatusTextChanged();
}

void AccountWizardController::setUserDisplayName(const QString &userDisplayName)
{
    if (_userDisplayName == userDisplayName) {
        return;
    }
    _userDisplayName = userDisplayName;
    emit userDisplayNameChanged();
}

void AccountWizardController::setServerDisplayName(const QString &serverDisplayName)
{
    if (_serverDisplayName == serverDisplayName) {
        return;
    }
    _serverDisplayName = serverDisplayName;
    emit serverDisplayNameChanged();
}

void AccountWizardController::setAvatarUrl(const QString &avatarUrl)
{
    if (_avatarUrl == avatarUrl) {
        return;
    }
    _avatarUrl = avatarUrl;
    emit avatarUrlChanged();
}

void AccountWizardController::setSyncEverythingDescription(const QString &syncEverythingDescription)
{
    if (_syncEverythingDescription == syncEverythingDescription) {
        return;
    }
    _syncEverythingDescription = syncEverythingDescription;
    emit syncEverythingDescriptionChanged();
}

void AccountWizardController::setNeedsSyncOptions(bool needsSyncOptions)
{
    if (_needsSyncOptions == needsSyncOptions) {
        return;
    }
    _needsSyncOptions = needsSyncOptions;
    emit needsSyncOptionsChanged();
}

void AccountWizardController::setPublicShareSetup(bool publicShareSetup)
{
    if (_publicShareSetup == publicShareSetup) {
        return;
    }

    _publicShareSetup = publicShareSetup;
    emit publicShareSetupChanged();
}

void AccountWizardController::setServerUrlEditable(bool editable)
{
    if (_serverUrlEditable == editable) {
        return;
    }
    _serverUrlEditable = editable;
    emit serverUrlEditableChanged();
}

void AccountWizardController::discardFlow2Auth()
{
    auto *oldAuth = _flow2Auth.release();
    if (oldAuth) {
        oldAuth->deleteLater();
    }
    setAuthPolling(false);
}

bool AccountWizardController::handleSecureConnectionFailure(QNetworkReply *reply, bool retryHttpOnly)
{
    const auto failedUrl = _account ? _account->url() : reply ? reply->url() : QUrl{};
    if (failedUrl.scheme() != "https"_L1 || !_account) {
        return false;
    }

    _secureConnectionFailedUrl = failedUrl;
    emit secureConnectionFailed(failedUrl.host(), retryHttpOnly);
    return true;
}

void AccountWizardController::retrySecureConnectionWithoutTls()
{
    if (!_account || _secureConnectionFailedUrl.scheme() != "https"_L1) {
        return;
    }

    auto url = _secureConnectionFailedUrl;
    url.setScheme("http"_L1);
    _secureConnectionFailedUrl = QUrl();
    setServerUrl(url.toString());
    startServerCheck(url);
}

void AccountWizardController::useClientCertificateForSecureConnection()
{
    if (!_account) {
        return;
    }

    _secureConnectionFailedUrl = QUrl();
    emit clientCertificateDialogRequested();
}

void AccountWizardController::chooseClientCertificate()
{
    const auto fileUrl = QFileDialog::getOpenFileUrl(nullptr,
        tr("Select a certificate"),
        QUrl(),
        tr("Certificate files (*.p12 *.pfx)"));
    if (fileUrl.isEmpty()) {
        return;
    }

#ifdef Q_OS_MACOS
    const auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(fileUrl);
    if (!scopedAccess->isValid()) {
        _clientCertificateError = tr("Could not access the selected certificate file.");
        emit clientCertificateChanged();
        return;
    }
#endif

    _clientCertificatePath = fileUrl.toLocalFile();
    _clientCertificateError.clear();
    emit clientCertificateChanged();
}

bool AccountWizardController::submitClientCertificate()
{
    if (!_account) {
        return false;
    }

    QFile certFile(_clientCertificatePath);
    if (!certFile.open(QFile::ReadOnly)) {
        qCWarning(lcAccountWizardController) << "Failed to open certificate file:" << _clientCertificatePath;
        _clientCertificateError = tr("Could not access the selected certificate file.");
        emit clientCertificateChanged();
        return false;
    }

    const auto certData = certFile.readAll();
    const auto certPassword = _clientCertificatePassword.toLocal8Bit();

    auto certDataCopy = certData;
    QBuffer certDataBuffer(&certDataCopy);
    certDataBuffer.open(QIODevice::ReadOnly);
    if (!QSslCertificate::importPkcs12(&certDataBuffer,
            &_clientSslKey,
            &_clientSslCertificate,
            &_clientSslCaCertificates,
            certPassword)) {
        _clientCertificateError = tr("Could not load certificate. Maybe wrong password?");
        emit clientCertificateChanged();
        return false;
    }

    _clientCertBundle = certData;
    _clientCertPassword = certPassword;

    clearClientCertificateInput();
    startServerCheck(_account->url());
    return true;
}

void AccountWizardController::clearClientCertificateInput()
{
    _clientCertificatePath.clear();
    _clientCertificatePassword.clear();
    _clientCertificateError.clear();
    emit clientCertificateChanged();
}

bool AccountWizardController::checkDowngradeAdvised(QNetworkReply *reply) const
{
    if (!reply || reply->url().scheme() != "https"_L1) {
        return false;
    }

    switch (reply->error()) {
    case QNetworkReply::NoError:
    case QNetworkReply::ContentNotFoundError:
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::HostNotFoundError:
        return false;
    default:
        break;
    }

    return !reply->hasRawHeader("Strict-Transport-Security"_L1);
}

} // namespace OCC

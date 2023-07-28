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

#include "account.h"
#include "accessmanager.h"
#include "accountfwd.h"
#include "capabilities.h"
#include "clientsideencryptionjobs.h"
#include "configfile.h"
#include "cookiejar.h"
#include "creds/abstractcredentials.h"
#include "networkjobs.h"
#include "pushnotifications.h"
#include "theme.h"
#include "version.h"

#include "deletejob.h"
#include "lockfilejobs.h"

#include "common/syncjournaldb.h"
#include "common/asserts.h"
#include "clientsideencryption.h"
#include "ocsuserstatusconnector.h"

#include "config.h"

#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QSslSocket>
#include <QNetworkCookieJar>
#include <QNetworkProxy>

#include <QFileInfo>
#include <QDir>
#include <QSslKey>
#include <QAuthenticator>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QHttpMultiPart>

#include <qsslconfiguration.h>

#include <qt6keychain/keychain.h>

#include "creds/abstractcredentials.h"

using namespace QKeychain;

namespace {
constexpr int pushNotificationsReconnectInterval = 1000 * 60 * 2;
constexpr int usernamePrefillServerVersionMinSupportedMajor = 24;
constexpr int checksumRecalculateRequestServerVersionMinSupportedMajor = 24;
constexpr auto isSkipE2eeMetadataChecksumValidationAllowedInClientVersion = MIRALL_VERSION_MAJOR == 3 && MIRALL_VERSION_MINOR == 8;
}

namespace OCC {
Q_LOGGING_CATEGORY(lcAccount, "nextcloud.sync.account", QtInfoMsg)
const char app_password[] = "_app-password";

Account::Account(QObject *parent)
    : QObject(parent)
    , _capabilities(QVariantMap())
    , _serverColor(Theme::defaultColor())
{
    qRegisterMetaType<AccountPtr>("AccountPtr");
    qRegisterMetaType<Account *>("Account*");

    _pushNotificationsReconnectTimer.setInterval(pushNotificationsReconnectInterval);
    connect(&_pushNotificationsReconnectTimer, &QTimer::timeout, this, &Account::trySetupPushNotifications);

    connect(&_e2e, &ClientSideEncryption::userCertificateNeedsMigrationChanged,
            this, &Account::userCertificateNeedsMigrationChanged);
}

AccountPtr Account::create()
{
    AccountPtr acc = AccountPtr(new Account);
    acc->setSharedThis(acc);
    return acc;
}

ClientSideEncryption* Account::e2e()
{
    // Qt expects everything in the connect to be a pointer, so return a pointer.
    return &_e2e;
}

Account::~Account() = default;

QString Account::davPath() const
{
    return davPathRoot() + QLatin1Char('/');
}

QString Account::davPathRoot() const
{
    return davPathBase() + QLatin1Char('/') + davUser();
}

void Account::setSharedThis(AccountPtr sharedThis)
{
    _sharedThis = sharedThis.toWeakRef();
    setupUserStatusConnector();
}

QString Account::davPathBase()
{
    return QStringLiteral("/remote.php/dav/files");
}

AccountPtr Account::sharedFromThis()
{
    return _sharedThis.toStrongRef();
}

AccountPtr Account::sharedFromThis() const
{
    return _sharedThis.toStrongRef();
}

QString Account::davUser() const
{
    return _davUser.isEmpty() && _credentials ? _credentials->user() : _davUser;
}

void Account::setDavUser(const QString &newDavUser)
{
    if (_davUser == newDavUser) {
        return;
    }

    _davUser = newDavUser;

    emit wantsAccountSaved(this);
    emit prettyNameChanged();
}

#ifndef TOKEN_AUTH_ONLY
QImage Account::avatar() const
{
    return _avatarImg;
}
void Account::setAvatar(const QImage &img)
{
    _avatarImg = img;
    emit accountChangedAvatar();
}
#endif

QString Account::displayName() const
{
    auto credentialsUser = _davUser;
    if (_credentials && !_credentials->user().isEmpty()) {
        credentialsUser = _credentials->user();
    }

    auto displayName = QStringLiteral("%1@%2").arg(credentialsUser, _url.host());
    const auto port = url().port();
    if (port > 0 && port != 80 && port != 443) {
        displayName.append(QLatin1Char(':'));
        displayName.append(QString::number(port));
    }

    return displayName;
}

QString Account::userIdAtHostWithPort() const
{
    QString dn = QStringLiteral("%1@%2").arg(_davUser, _url.host());
    const auto port = url().port();
    if (port > 0 && port != 80 && port != 443) {
        dn.append(QLatin1Char(':'));
        dn.append(QString::number(port));
    }
    return dn;
}

QString Account::davDisplayName() const
{
    return _davDisplayName;
}

void Account::setDavDisplayName(const QString &newDisplayName)
{
    _davDisplayName = newDisplayName;
    emit accountChangedDisplayName();
    emit prettyNameChanged();
}

QString Account::prettyName() const
{
    // If davDisplayName is empty (can be several reasons, simplest is missing login at startup), fall back to username
    auto name = davDisplayName();

    if (name.isEmpty()) {
        name = davUser();
    }

    return name;
}

QColor Account::serverColor() const
{
    return _serverColor;
}

QColor Account::headerColor() const
{
    return serverColor();
}

QColor Account::headerTextColor() const
{
    return _serverTextColor;
}

QColor Account::accentColor() const
{
    const auto accentColor = serverColor();
    constexpr auto effectMultiplier = 8;

    auto darknessAdjustment = static_cast<int>((1 - Theme::getColorDarkness(accentColor)) * effectMultiplier);
    darknessAdjustment *= darknessAdjustment; // Square the value to pronounce the darkness more in lighter colours
    constexpr auto baseAdjustment = 125;
    const auto adjusted = Theme::isDarkColor(accentColor) ? accentColor : accentColor.darker(baseAdjustment + darknessAdjustment);
    return adjusted;
}

QString Account::id() const
{
    return _id;
}

AbstractCredentials *Account::credentials() const
{
    return _credentials.data();
}

void Account::setCredentials(AbstractCredentials *cred)
{
    // set active credential manager
    QNetworkCookieJar *jar = nullptr;
    QNetworkProxy proxy;

    if (_networkAccessManager) {
        jar = _networkAccessManager->cookieJar();
        jar->setParent(nullptr);

        // Remember proxy (issue #2108)
        proxy = _networkAccessManager->proxy();

        _networkAccessManager = QSharedPointer<QNetworkAccessManager>();
    }

    // The order for these two is important! Reading the credential's
    // settings accesses the account as well as account->_credentials,
    _credentials.reset(cred);
    cred->setAccount(this);

    // Note: This way the QNAM can outlive the Account and Credentials.
    // This is necessary to avoid issues with the QNAM being deleted while
    // processing slotHandleSslErrors().
    _networkAccessManager = QSharedPointer<QNetworkAccessManager>(_credentials->createQNAM(), &QObject::deleteLater);

    if (jar) {
        _networkAccessManager->setCookieJar(jar);
    }
    if (proxy.type() != QNetworkProxy::DefaultProxy) {
        _networkAccessManager->setProxy(proxy);
    }
    connect(_networkAccessManager.data(), &QNetworkAccessManager::sslErrors,
        this, &Account::slotHandleSslErrors);
    connect(_networkAccessManager.data(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &Account::proxyAuthenticationRequired);
    connect(_credentials.data(), &AbstractCredentials::fetched,
        this, &Account::slotCredentialsFetched);
    connect(_credentials.data(), &AbstractCredentials::asked,
        this, &Account::slotCredentialsAsked);

    trySetupPushNotifications();
}

void Account::setPushNotificationsReconnectInterval(int interval)
{
    _pushNotificationsReconnectTimer.setInterval(interval);
}

void Account::trySetupClientStatusReporting()
{
    if (!_capabilities.isClientStatusReportingEnabled()) {
        _clientStatusReporting.reset();
        return;
    }

    if (!_clientStatusReporting) {
        _clientStatusReporting = std::make_unique<ClientStatusReporting>(this);
    }
}

void Account::reportClientStatus(const ClientStatusReportingStatus status) const
{
    if (_clientStatusReporting) {
        _clientStatusReporting->reportClientStatus(status);
    }
}

void Account::trySetupPushNotifications()
{
    // Stop the timer to prevent parallel setup attempts
    _pushNotificationsReconnectTimer.stop();

    if (_capabilities.availablePushNotifications() != PushNotificationType::None) {
        qCInfo(lcAccount) << "Try to setup push notifications";

        if (!_pushNotifications) {
            _pushNotifications = new PushNotifications(this, this);

            connect(_pushNotifications, &PushNotifications::ready, this, [this]() {
                _pushNotificationsReconnectTimer.stop();
                emit pushNotificationsReady(this);
            });

            const auto disablePushNotifications = [this]() {
                qCInfo(lcAccount) << "Disable push notifications object because authentication failed or connection lost";
                if (!_pushNotifications) {
                    return;
                }
                if (!_pushNotifications->isReady()) {
                    emit pushNotificationsDisabled(this);
                }
                if (!_pushNotificationsReconnectTimer.isActive()) {
                    _pushNotificationsReconnectTimer.start();
                }
            };

            connect(_pushNotifications, &PushNotifications::connectionLost, this, disablePushNotifications);
            connect(_pushNotifications, &PushNotifications::authenticationFailed, this, disablePushNotifications);
        }
        // If push notifications already running it is no problem to call setup again
        _pushNotifications->setup();
    }
}

QUrl Account::davUrl() const
{
    return Utility::concatUrlPath(url(), davPath());
}

QUrl Account::deprecatedPrivateLinkUrl(const QByteArray &numericFileId) const
{
    return Utility::concatUrlPath(_userVisibleUrl,
        QLatin1String("/index.php/f/") + QUrl::toPercentEncoding(QString::fromLatin1(numericFileId)));
}

/**
 * clear all cookies. (Session cookies or not)
 */
void Account::clearCookieJar()
{
    const auto jar = qobject_cast<CookieJar *>(_networkAccessManager->cookieJar());
    ASSERT(jar);
    jar->setAllCookies(QList<QNetworkCookie>());
}

/*! This shares our official cookie jar (containing all the tasty
    authentication cookies) with another QNAM while making sure
    of not losing its ownership. */
void Account::lendCookieJarTo(QNetworkAccessManager *guest)
{
    auto jar = _networkAccessManager->cookieJar();
    auto oldParent = jar->parent();
    guest->setCookieJar(jar); // takes ownership of our precious cookie jar
    jar->setParent(oldParent); // takes it back
}

QString Account::cookieJarPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/cookies" + id() + ".db";
}

void Account::resetNetworkAccessManager()
{
    if (!_credentials || !_networkAccessManager) {
        return;
    }

    qCDebug(lcAccount) << "Resetting QNAM";
    QNetworkCookieJar *jar = _networkAccessManager->cookieJar();
    QNetworkProxy proxy = _networkAccessManager->proxy();

    // Use a QSharedPointer to allow locking the life of the QNAM on the stack.
    // Make it call deleteLater to make sure that we can return to any QNAM stack frames safely.
    _networkAccessManager = QSharedPointer<QNetworkAccessManager>(_credentials->createQNAM(), &QObject::deleteLater);

    _networkAccessManager->setCookieJar(jar); // takes ownership of the old cookie jar
    _networkAccessManager->setProxy(proxy);   // Remember proxy (issue #2108)

    connect(_networkAccessManager.data(), &QNetworkAccessManager::sslErrors,
        this, &Account::slotHandleSslErrors);
    connect(_networkAccessManager.data(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &Account::proxyAuthenticationRequired);
}

QNetworkAccessManager *Account::networkAccessManager()
{
    return _networkAccessManager.data();
}

QSharedPointer<QNetworkAccessManager> Account::sharedNetworkAccessManager()
{
    return _networkAccessManager;
}

QNetworkReply *Account::sendRawRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    req.setUrl(url);
    req.setSslConfiguration(this->getOrCreateSslConfig());
    if (verb == "HEAD" && !data) {
        return _networkAccessManager->head(req);
    } else if (verb == "GET" && !data) {
        return _networkAccessManager->get(req);
    } else if (verb == "POST") {
        return _networkAccessManager->post(req, data);
    } else if (verb == "PUT") {
        return _networkAccessManager->put(req, data);
    } else if (verb == "DELETE" && !data) {
        return _networkAccessManager->deleteResource(req);
    }
    return _networkAccessManager->sendCustomRequest(req, verb, data);
}

QNetworkReply *Account::sendRawRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, const QByteArray &data)
{
    req.setUrl(url);
    req.setSslConfiguration(this->getOrCreateSslConfig());
    if (verb == "HEAD" && data.isEmpty()) {
        return _networkAccessManager->head(req);
    } else if (verb == "GET" && data.isEmpty()) {
        return _networkAccessManager->get(req);
    } else if (verb == "POST") {
        return _networkAccessManager->post(req, data);
    } else if (verb == "PUT") {
        return _networkAccessManager->put(req, data);
    } else if (verb == "DELETE" && data.isEmpty()) {
        return _networkAccessManager->deleteResource(req);
    }
    return _networkAccessManager->sendCustomRequest(req, verb, data);
}

QNetworkReply *Account::sendRawRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QHttpMultiPart *data)
{
    req.setUrl(url);
    req.setSslConfiguration(this->getOrCreateSslConfig());
    if (verb == "PUT") {
        return _networkAccessManager->put(req, data);
    } else if (verb == "POST") {
        return _networkAccessManager->post(req, data);
    }
    return _networkAccessManager->sendCustomRequest(req, verb, data);
}

SimpleNetworkJob *Account::sendRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    auto job = new SimpleNetworkJob(sharedFromThis());
    job->startRequest(verb, url, req, data);
    return job;
}

void Account::setSslConfiguration(const QSslConfiguration &config)
{
    _sslConfiguration = config;
}

QSslConfiguration Account::getOrCreateSslConfig()
{
    if (!_sslConfiguration.isNull()) {
        // Will be set by CheckServerJob::finished()
        // We need to use a central shared config to get SSL session tickets
        return _sslConfiguration;
    }

    // if setting the client certificate fails, you will probably get an error similar to this:
    //  "An internal error number 1060 happened. SSL handshake failed, client certificate was requested: SSL error: sslv3 alert handshake failure"
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();

    // Try hard to reuse session for different requests
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionTickets, false);
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionSharing, false);
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionPersistence, false);

    sslConfig.setOcspStaplingEnabled(Theme::instance()->enableStaplingOCSP());

    return sslConfig;
}

void Account::setApprovedCerts(const QList<QSslCertificate> certs)
{
    _approvedCerts = certs;
    QSslConfiguration::defaultConfiguration().addCaCertificates(certs);
}

void Account::addApprovedCerts(const QList<QSslCertificate> certs)
{
    _approvedCerts += certs;
}

void Account::resetRejectedCertificates()
{
    _rejectedCertificates.clear();
}

void Account::setSslErrorHandler(AbstractSslErrorHandler *handler)
{
    _sslErrorHandler.reset(handler);
}

void Account::setUrl(const QUrl &url)
{
    _url = url;
    _userVisibleUrl = url;
}

void Account::setUserVisibleHost(const QString &host)
{
    _userVisibleUrl.setHost(host);
}

QVariant Account::credentialSetting(const QString &key) const
{
    if (_credentials) {
        const auto prefix = _credentials->authType();
        auto value = _settingsMap.value(prefix + "_" + key);
        if (value.isNull()) {
            value = _settingsMap.value(key);
        }

        return value;
    }
    return QVariant();
}

void Account::setCredentialSetting(const QString &key, const QVariant &value)
{
    if (_credentials) {
        QString prefix = _credentials->authType();
        _settingsMap.insert(prefix + "_" + key, value);
    }
}

void Account::slotHandleSslErrors(QNetworkReply *reply, QList<QSslError> errors)
{
    NetworkJobTimeoutPauser pauser(reply);
    QString out;
    QDebug(&out) << "SSL-Errors happened for url " << reply->url().toString();
    foreach (const QSslError &error, errors) {
        QDebug(&out) << "\tError in " << error.certificate() << ":"
                     << error.errorString() << "(" << error.error() << ")"
                     << "\n";
    }

    qCInfo(lcAccount()) << "ssl errors" << out;
    qCInfo(lcAccount()) << reply->sslConfiguration().peerCertificateChain();

    bool allPreviouslyRejected = true;
    foreach (const QSslError &error, errors) {
        if (!_rejectedCertificates.contains(error.certificate())) {
            allPreviouslyRejected = false;
        }
    }

    // If all certs have previously been rejected by the user, don't ask again.
    if (allPreviouslyRejected) {
        qCInfo(lcAccount) << out << "Certs not trusted by user decision, returning.";
        return;
    }

    QList<QSslCertificate> approvedCerts;
    if (_sslErrorHandler.isNull()) {
        qCWarning(lcAccount) << out << "called without valid SSL error handler for account" << url();
        return;
    }

    // SslDialogErrorHandler::handleErrors will run an event loop that might execute
    // the deleteLater() of the QNAM before we have the chance of unwinding our stack.
    // Keep a ref here on our stackframe to make sure that it doesn't get deleted before
    // handleErrors returns.
    QSharedPointer<QNetworkAccessManager> qnamLock = _networkAccessManager;
    QPointer<QObject> guard = reply;

    if (_sslErrorHandler->handleErrors(errors, reply->sslConfiguration(), &approvedCerts, sharedFromThis())) {
        if (!guard)
            return;

        if (!approvedCerts.isEmpty()) {
            QSslConfiguration::defaultConfiguration().addCaCertificates(approvedCerts);
            addApprovedCerts(approvedCerts);
            emit wantsAccountSaved(this);

            // all ssl certs are known and accepted. We can ignore the problems right away.
            qCInfo(lcAccount) << out << "Certs are known and trusted! This is not an actual error.";
        }

        // Warning: Do *not* use ignoreSslErrors() (without args) here:
        // it permanently ignores all SSL errors for this host, even
        // certificate changes.
        reply->ignoreSslErrors(errors);
    } else {
        if (!guard)
            return;

        // Mark all involved certificates as rejected, so we don't ask the user again.
        foreach (const QSslError &error, errors) {
            if (!_rejectedCertificates.contains(error.certificate())) {
                _rejectedCertificates.append(error.certificate());
            }
        }

        // Not calling ignoreSslErrors will make the SSL handshake fail.
        return;
    }
}

void Account::slotCredentialsFetched()
{
    if (_davUser.isEmpty()) {
        qCDebug(lcAccount) << "User id not set. Fetch it.";
        const auto fetchUserNameJob = new JsonApiJob(sharedFromThis(), QStringLiteral("/ocs/v1.php/cloud/user"));
        connect(fetchUserNameJob, &JsonApiJob::jsonReceived, this, [this, fetchUserNameJob](const QJsonDocument &json, int statusCode) {
            fetchUserNameJob->deleteLater();
            if (statusCode != 100) {
                qCWarning(lcAccount) << "Could not fetch user id. Login will probably not work.";
                emit credentialsFetched(_credentials.data());
                return;
            }

            const auto objData = json.object().value("ocs").toObject().value("data").toObject();
            const auto userId = objData.value("id").toString("");
            setDavUser(userId);
            emit credentialsFetched(_credentials.data());
        });
        fetchUserNameJob->start();
    } else {
        qCDebug(lcAccount) << "User id already fetched.";
        emit credentialsFetched(_credentials.data());
    }
}

void Account::slotCredentialsAsked()
{
    emit credentialsAsked(_credentials.data());
}

void Account::handleInvalidCredentials()
{
    // Retrieving password will trigger remote wipe check job
    retrieveAppPassword();

    emit invalidCredentials();
}

void Account::clearQNAMCache()
{
    _networkAccessManager->clearAccessCache();
}

const Capabilities &Account::capabilities() const
{
    return _capabilities;
}

void Account::updateServerColors()
{
    if (const auto capServerColor = _capabilities.serverColor(); capServerColor.isValid()) {
        _serverColor = capServerColor;
    }

    if (const auto capServerTextColor = _capabilities.serverTextColor(); capServerTextColor.isValid()) {
        _serverTextColor = capServerTextColor;
    }
}

void Account::setCapabilities(const QVariantMap &caps)
{
    _capabilities = Capabilities(caps);

    updateServerColors();
    updateServerSubcription();
    updateDesktopEnterpriseChannel();

    emit capabilitiesChanged();

    setupUserStatusConnector();
    trySetupPushNotifications();

    trySetupClientStatusReporting();
}

void Account::setupUserStatusConnector()
{
    _userStatusConnector = std::make_shared<OcsUserStatusConnector>(sharedFromThis());
    connect(_userStatusConnector.get(), &UserStatusConnector::userStatusFetched, this, [this](const UserStatus &) {
        emit userStatusChanged();
    });
    connect(_userStatusConnector.get(), &UserStatusConnector::serverUserStatusChanged, this, &Account::serverUserStatusChanged);
    connect(_userStatusConnector.get(), &UserStatusConnector::messageCleared, this, [this] {
        emit userStatusChanged();
    });

    _userStatusConnector->fetchUserStatus();
}

QString Account::serverVersion() const
{
    return _serverVersion;
}

bool Account::shouldSkipE2eeMetadataChecksumValidation() const
{
    return isSkipE2eeMetadataChecksumValidationAllowedInClientVersion && _skipE2eeMetadataChecksumValidation;
}

void Account::resetShouldSkipE2eeMetadataChecksumValidation()
{
    _skipE2eeMetadataChecksumValidation = false;
    emit wantsAccountSaved(this);
}

int Account::serverVersionInt() const
{
    // FIXME: Use Qt 5.5 QVersionNumber
    auto components = serverVersion().split('.');
    return makeServerVersion(components.value(0).toInt(),
        components.value(1).toInt(),
        components.value(2).toInt());
}

bool Account::serverHasMountRootProperty() const
{
    if (serverVersionInt() == 0) {
        return false;
    }

    return serverVersionInt() >= Account::makeServerVersion(NEXTCLOUD_SERVER_VERSION_MOUNT_ROOT_PROPERTY_SUPPORTED_MAJOR,
                                                            NEXTCLOUD_SERVER_VERSION_MOUNT_ROOT_PROPERTY_SUPPORTED_MINOR,
                                                            NEXTCLOUD_SERVER_VERSION_MOUNT_ROOT_PROPERTY_SUPPORTED_PATCH);
}

bool Account::serverVersionUnsupported() const
{
    if (serverVersionInt() == 0) {
        // not detected yet, assume it is fine.
        return false;
    }
    return serverVersionInt() < makeServerVersion(NEXTCLOUD_SERVER_VERSION_MIN_SUPPORTED_MAJOR,
               NEXTCLOUD_SERVER_VERSION_MIN_SUPPORTED_MINOR, NEXTCLOUD_SERVER_VERSION_MIN_SUPPORTED_PATCH);
}

bool Account::secureFileDropSupported() const
{
    if (serverVersionInt() == 0) {
        // not detected yet, assume it is fine.
        return true;
    }
    return serverVersionInt() >= makeServerVersion(NEXTCLOUD_SERVER_VERSION_SECURE_FILEDROP_MIN_SUPPORTED_MAJOR,
                                                   NEXTCLOUD_SERVER_VERSION_SECURE_FILEDROP_MIN_SUPPORTED_MINOR,
                                                   NEXTCLOUD_SERVER_VERSION_SECURE_FILEDROP_MIN_SUPPORTED_PATCH);
}

bool Account::isUsernamePrefillSupported() const
{
    return serverVersionInt() >= makeServerVersion(usernamePrefillServerVersionMinSupportedMajor, 0, 0);
}

bool Account::isChecksumRecalculateRequestSupported() const
{
    return serverVersionInt() >= makeServerVersion(checksumRecalculateRequestServerVersionMinSupportedMajor, 0, 0);
}

int Account::checksumRecalculateServerVersionMinSupportedMajor() const
{
    return checksumRecalculateRequestServerVersionMinSupportedMajor;
}

void Account::setServerVersion(const QString &version)
{
    if (version == _serverVersion) {
        return;
    }

    auto oldServerVersion = _serverVersion;
    _serverVersion = version;
    emit serverVersionChanged(this, oldServerVersion, version);
}

void Account::writeAppPasswordOnce(QString appPassword){
    if(_wroteAppPassword)
        return;

    // Fix: Password got written from Account Wizard, before finish.
    // Only write the app password for a connected account, else
    // there'll be a zombie keychain slot forever, never used again ;p
    //
    // Also don't write empty passwords (Log out -> Relaunch)
    if(id().isEmpty() || appPassword.isEmpty())
        return;

    const QString kck = AbstractCredentials::keychainKey(
                url().toString(),
                davUser() + app_password,
                id()
    );

    auto *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->setBinaryData(appPassword.toLatin1());
    connect(job, &WritePasswordJob::finished, [this](Job *incoming) {
        auto *writeJob = dynamic_cast<WritePasswordJob *>(incoming);
        if (writeJob->error() == NoError)
            qCInfo(lcAccount) << "appPassword stored in keychain";
        else
            qCWarning(lcAccount) << "Unable to store appPassword in keychain" << writeJob->errorString();

        // We don't try this again on error, to not raise CPU consumption
        _wroteAppPassword = true;
    });
    job->start();
}

void Account::retrieveAppPassword(){
    const QString key = credentials()->user() + app_password;
    const QString kck = AbstractCredentials::keychainKey(
                url().toString(),
                key,
                id()
    );

    auto *job = new ReadPasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &ReadPasswordJob::finished, [this](Job *incoming) {
        auto *readJob = dynamic_cast<ReadPasswordJob *>(incoming);
        QString pwd("");
        // Error or no valid public key error out
        if (readJob->error() == NoError &&
                readJob->binaryData().length() > 0) {
            pwd = readJob->binaryData();
        }

        emit appPasswordRetrieved(pwd);
    });
    job->start();
}

void Account::deleteAppPassword()
{
    const QString kck = AbstractCredentials::keychainKey(
                url().toString(),
                credentials()->user() + app_password,
                id()
    );

    if (kck.isEmpty()) {
        qCDebug(lcAccount) << "appPassword is empty";
        return;
    }

    auto *job = new DeletePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &DeletePasswordJob::finished, [this](Job *incoming) {
        auto *deleteJob = dynamic_cast<DeletePasswordJob *>(incoming);
        const auto jobError = deleteJob->error();
        if (jobError == NoError) {
            qCInfo(lcAccount) << "appPassword deleted from keychain";
        } else if (jobError == EntryNotFound) {
            qCInfo(lcAccount) << "no appPassword entry found";
        } else {
            qCWarning(lcAccount) << "Unable to delete appPassword from keychain" << deleteJob->errorString();
        }

        // Allow storing a new app password on re-login
        _wroteAppPassword = false;
    });
    job->start();
}

void Account::deleteAppToken()
{
    const auto deleteAppTokenJob = new DeleteJob(sharedFromThis(), QStringLiteral("/ocs/v2.php/core/apppassword"));
    connect(deleteAppTokenJob, &DeleteJob::finishedSignal, this, [this]() {
        if (const auto deleteJob = qobject_cast<DeleteJob *>(QObject::sender())) {
            const auto httpCode = deleteJob->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (httpCode != 200) {
                qCWarning(lcAccount) << "AppToken remove failed for user: " << displayName() << " with code: " << httpCode;
            } else {
                qCInfo(lcAccount) << "AppToken for user: " << displayName() << " has been removed.";
            }
        } else {
            Q_ASSERT(false);
            qCWarning(lcAccount) << "The sender is not a DeleteJob instance.";
        }
    });
    deleteAppTokenJob->start();
}

void Account::fetchDirectEditors(const QUrl &directEditingURL, const QString &directEditingETag)
{
    if(directEditingURL.isEmpty() || directEditingETag.isEmpty())
        return;

    // Check for the directEditing capability
    if (!directEditingURL.isEmpty() &&
        (directEditingETag.isEmpty() || directEditingETag != _lastDirectEditingETag)) {
            // Fetch the available editors and their mime types
            auto *job = new JsonApiJob(sharedFromThis(), QLatin1String("ocs/v2.php/apps/files/api/v1/directEditing"));
            QObject::connect(job, &JsonApiJob::jsonReceived, this, &Account::slotDirectEditingRecieved);
            job->start();
    }
}

void Account::slotDirectEditingRecieved(const QJsonDocument &json)
{
    auto data = json.object().value("ocs").toObject().value("data").toObject();
    auto editors = data.value("editors").toObject();

    foreach (auto editorKey, editors.keys()) {
        auto editor = editors.value(editorKey).toObject();

        const QString id = editor.value("id").toString();
        const QString name = editor.value("name").toString();

        if(!id.isEmpty() && !name.isEmpty()) {
            auto mimeTypes = editor.value("mimetypes").toArray();
            auto optionalMimeTypes = editor.value("optionalMimetypes").toArray();

            auto *directEditor = new DirectEditor(id, name);

            foreach(auto mimeType, mimeTypes) {
                directEditor->addMimetype(mimeType.toString().toLatin1());
            }

            foreach(auto optionalMimeType, optionalMimeTypes) {
                directEditor->addOptionalMimetype(optionalMimeType.toString().toLatin1());
            }

            _capabilities.addDirectEditor(directEditor);
        }
    }
}

void Account::removeLockStatusChangeInprogress(const QString &serverRelativePath, const SyncFileItem::LockStatus lockStatus)
{
    const auto foundLockStatusJobInProgress = _lockStatusChangeInprogress.find(serverRelativePath);
    if (foundLockStatusJobInProgress != _lockStatusChangeInprogress.end()) {
        foundLockStatusJobInProgress.value().removeAll(lockStatus);
        if (foundLockStatusJobInProgress.value().isEmpty()) {
            _lockStatusChangeInprogress.erase(foundLockStatusJobInProgress);
        }
    }
}

PushNotifications *Account::pushNotifications() const
{
    return _pushNotifications;
}

std::shared_ptr<UserStatusConnector> Account::userStatusConnector() const
{
    return _userStatusConnector;
}

void Account::setLockFileState(const QString &serverRelativePath,
                               const QString &remoteSyncPathWithTrailingSlash,
                               const QString &localSyncPath,
                               const QString &etag,
                               SyncJournalDb * const journal,
                               const SyncFileItem::LockStatus lockStatus,
                               const SyncFileItem::LockOwnerType lockOwnerType)
{
    auto& lockStatusJobInProgress = _lockStatusChangeInprogress[serverRelativePath];
    if (lockStatusJobInProgress.contains(lockStatus)) {
        qCWarning(lcAccount) << "Already running a job with lockStatus:" << lockStatus << " for: " << serverRelativePath;
        return;
    }
    lockStatusJobInProgress.push_back(lockStatus);
    auto job = std::make_unique<LockFileJob>(sharedFromThis(), journal, serverRelativePath, remoteSyncPathWithTrailingSlash, localSyncPath, etag, lockStatus, lockOwnerType);
    connect(job.get(), &LockFileJob::finishedWithoutError, this, [this, serverRelativePath, lockStatus]() {
        removeLockStatusChangeInprogress(serverRelativePath, lockStatus);
        Q_EMIT lockFileSuccess();
    });
    connect(job.get(), &LockFileJob::finishedWithError, this, [lockStatus, serverRelativePath, this](const int httpErrorCode, const QString &errorString, const QString &lockOwnerName) {
        removeLockStatusChangeInprogress(serverRelativePath, lockStatus);
        auto errorMessage = QString{};
        const auto filePath = serverRelativePath.mid(1);

        if (httpErrorCode == LockFileJob::LOCKED_HTTP_ERROR_CODE) {
            errorMessage = tr("File %1 is already locked by %2.").arg(filePath, lockOwnerName);
        } else if (lockStatus == SyncFileItem::LockStatus::LockedItem) {
             errorMessage = tr("Lock operation on %1 failed with error %2").arg(filePath, errorString);
        } else if (lockStatus == SyncFileItem::LockStatus::UnlockedItem) {
             errorMessage = tr("Unlock operation on %1 failed with error %2").arg(filePath, errorString);
        }
        Q_EMIT lockFileError(errorMessage);
    });
    job->start();
    static_cast<void>(job.release());
}

SyncFileItem::LockStatus Account::fileLockStatus(SyncJournalDb * const journal,
                                                 const QString &folderRelativePath) const
{
    SyncJournalFileRecord record;
    if (journal->getFileRecord(folderRelativePath, &record)) {
        return record._lockstate._locked ? SyncFileItem::LockStatus::LockedItem : SyncFileItem::LockStatus::UnlockedItem;
    }

    return SyncFileItem::LockStatus::UnlockedItem;
}

bool Account::fileCanBeUnlocked(SyncJournalDb * const journal,
                                const QString &folderRelativePath) const
{
    SyncJournalFileRecord record;
    if (journal->getFileRecord(folderRelativePath, &record)) {
        if (record._lockstate._lockOwnerType == static_cast<int>(SyncFileItem::LockOwnerType::AppLock)) {
            qCDebug(lcAccount()) << folderRelativePath << "cannot be unlocked: app lock";
            return false;
        }

        if (record._lockstate._lockOwnerType == static_cast<int>(SyncFileItem::LockOwnerType::UserLock) &&
            record._lockstate._lockOwnerId != sharedFromThis()->davUser()) {
            qCDebug(lcAccount()) << folderRelativePath << "cannot be unlocked: user lock from" << record._lockstate._lockOwnerId;
            return false;
        }

        if (record._lockstate._lockOwnerType == static_cast<int>(SyncFileItem::LockOwnerType::TokenLock) &&
            record._lockstate._lockToken.isEmpty()) {
            qCDebug(lcAccount()) << folderRelativePath << "cannot be unlocked: token lock without known token";
            return false;
        }

        return true;
    }
    return false;
}

void Account::setTrustCertificates(bool trustCertificates)
{
    _trustCertificates = trustCertificates;
}

bool Account::trustCertificates() const
{
    return _trustCertificates;
}

void Account::setE2eEncryptionKeysGenerationAllowed(bool allowed)
{
    _e2eEncryptionKeysGenerationAllowed = allowed;
}

[[nodiscard]] bool Account::e2eEncryptionKeysGenerationAllowed() const
{
    return _e2eEncryptionKeysGenerationAllowed;
}

bool Account::askUserForMnemonic() const
{
    return _e2eAskUserForMnemonic;
}

bool Account::enforceUseHardwareTokenEncryption() const
{
#if defined CLIENTSIDEENCRYPTION_ENFORCE_USB_TOKEN
    return CLIENTSIDEENCRYPTION_ENFORCE_USB_TOKEN;
#else
    return false;
#endif
}

QString Account::encryptionHardwareTokenDriverPath() const
{
#if defined ENCRYPTION_HARDWARE_TOKEN_DRIVER_PATH
    return ENCRYPTION_HARDWARE_TOKEN_DRIVER_PATH;
#else
    return {};
#endif
}

QByteArray Account::encryptionCertificateFingerprint() const
{
    return _encryptionCertificateFingerprint;
}

void Account::setEncryptionCertificateFingerprint(const QByteArray &fingerprint)
{
    if (_encryptionCertificateFingerprint == fingerprint) {
        return;
    }

    _encryptionCertificateFingerprint = fingerprint;
    _e2e.usbTokenInformation()->setSha256Fingerprint(fingerprint);
    Q_EMIT encryptionCertificateFingerprintChanged();
    Q_EMIT wantsAccountSaved(this);
}

void Account::setAskUserForMnemonic(const bool ask)
{
    _e2eAskUserForMnemonic = ask;
    emit askUserForMnemonicChanged();
}

bool Account::serverHasValidSubscription() const
{
    return _serverHasValidSubscription;
}

void Account::setServerHasValidSubscription(bool valid)
{
    _serverHasValidSubscription = valid;
}

void Account::updateServerSubcription()
{
    ConfigFile currentConfig;
    const auto capabilityValidSubscription = _capabilities.serverHasValidSubscription();
    const auto configValidSubscription = currentConfig.serverHasValidSubscription();
    if (capabilityValidSubscription != configValidSubscription && !configValidSubscription) {
        currentConfig.setServerHasValidSubscription(capabilityValidSubscription);
    }

    setServerHasValidSubscription(capabilityValidSubscription);
}

void Account::updateDesktopEnterpriseChannel()
{
    ConfigFile currentConfig;
    if (const auto desktopEnterpriseChannel = _capabilities.desktopEnterpriseChannel();
        desktopEnterpriseChannel != currentConfig.desktopEnterpriseChannel()) {
        currentConfig.setDesktopEnterpriseChannel(desktopEnterpriseChannel);
    }
}

Account::AccountNetworkProxySetting Account::networkProxySetting() const
{
    return _networkProxySetting;
}

void Account::setNetworkProxySetting(const AccountNetworkProxySetting setting)
{
    if (setting == _networkProxySetting) {
        return;
    }

    _networkProxySetting = setting;
    if (setting == AccountNetworkProxySetting::AccountSpecificProxy) {
        auto proxy = _networkAccessManager->proxy();
        proxy.setType(proxyType());
        proxy.setHostName(proxyHostName());
        proxy.setPort(proxyPort());
        proxy.setUser(proxyUser());
        proxy.setPassword(proxyPassword());
        _networkAccessManager->setProxy(proxy);
    } else {
        const auto proxy = QNetworkProxy::applicationProxy();
        _networkAccessManager->setProxy(proxy);
        setProxyType(proxy.type());
        setProxyHostName(proxy.hostName());
        setProxyPort(proxy.port());
        setProxyUser(proxy.user());
        setProxyPassword(proxy.password());
    }
    emit networkProxySettingChanged();
}

QNetworkProxy::ProxyType Account::proxyType() const
{
    return _proxyType;
}

void Account::setProxyType(QNetworkProxy::ProxyType proxyType)
{
    if (_proxyType == proxyType) {
        return;
    }

    _proxyType = proxyType;

    if (networkProxySetting() == AccountNetworkProxySetting::AccountSpecificProxy) {
        auto proxy = _networkAccessManager->proxy();
        proxy.setType(proxyType);
        _networkAccessManager->setProxy(proxy);
    }

    emit proxyTypeChanged();
}

QString Account::proxyHostName() const
{
    return _proxyHostName;
}

void Account::setProxyHostName(const QString &hostName)
{
    if (_proxyHostName == hostName) {
        return;
    }

    _proxyHostName = hostName;

    if (networkProxySetting() == AccountNetworkProxySetting::AccountSpecificProxy) {
        auto proxy = _networkAccessManager->proxy();
        proxy.setHostName(hostName);
        _networkAccessManager->setProxy(proxy);
    }

    emit proxyHostNameChanged();
}

int Account::proxyPort() const
{
    return _proxyPort;
}

void Account::setProxyPort(const int port)
{
    if (_proxyPort == port) {
        return;
    }

    _proxyPort = port;

    if (networkProxySetting() == AccountNetworkProxySetting::AccountSpecificProxy) {
        auto proxy = _networkAccessManager->proxy();
        proxy.setPort(port);
        _networkAccessManager->setProxy(proxy);
    }

    emit proxyPortChanged();
}

bool Account::proxyNeedsAuth() const
{
    return _proxyNeedsAuth;
}

void Account::setProxyNeedsAuth(const bool needsAuth)
{
    if (_proxyNeedsAuth == needsAuth) {
        return;
    }

    _proxyNeedsAuth = needsAuth;
    emit proxyNeedsAuthChanged();
}

QString Account::proxyUser() const
{
    return _proxyUser;
}

void Account::setProxyUser(const QString &user)
{
    if (_proxyUser == user) {
        return;
    }

    _proxyUser = user;

    if (networkProxySetting() == AccountNetworkProxySetting::AccountSpecificProxy) {
        auto proxy = _networkAccessManager->proxy();
        proxy.setUser(user);
        _networkAccessManager->setProxy(proxy);
    }

    emit proxyUserChanged();
}

QString Account::proxyPassword() const
{
    return _proxyPassword;
}

void Account::setProxyPassword(const QString &password)
{
    if (_proxyPassword == password) {
        return;
    }

    _proxyPassword = password;

    if (networkProxySetting() == AccountNetworkProxySetting::AccountSpecificProxy) {
        auto proxy = _networkAccessManager->proxy();
        proxy.setPassword(password);
        _networkAccessManager->setProxy(proxy);
    }

    emit proxyPasswordChanged();
}

void Account::setProxySettings(const AccountNetworkProxySetting networkProxySetting,
                               const QNetworkProxy::ProxyType proxyType,
                               const QString &hostName,
                               const int port,
                               const bool needsAuth,
                               const QString &user,
                               const QString &password)
{
    if (networkProxySetting == AccountNetworkProxySetting::GlobalProxy) {
        setNetworkProxySetting(networkProxySetting);
        return;
    }

    setProxyType(proxyType);
    setProxyHostName(hostName);
    setProxyPort(port);
    setProxyNeedsAuth(needsAuth);
    setProxyUser(user);
    setProxyPassword(password);
    setNetworkProxySetting(networkProxySetting);
}

Account::AccountNetworkTransferLimitSetting Account::uploadLimitSetting() const
{
    return _uploadLimitSetting;
}

void Account::setUploadLimitSetting(const AccountNetworkTransferLimitSetting setting)
{
    if (setting == _uploadLimitSetting) {
        return;
    }

    _uploadLimitSetting = setting;
    emit uploadLimitSettingChanged();
}

Account::AccountNetworkTransferLimitSetting Account::downloadLimitSetting() const
{
    return _downloadLimitSetting;
}

void Account::setDownloadLimitSetting(const AccountNetworkTransferLimitSetting setting)
{
    if (setting == _downloadLimitSetting) {
        return;
    }
    
    _downloadLimitSetting = setting;
    emit downloadLimitSettingChanged();
}

unsigned int Account::uploadLimit() const
{
    return _uploadLimit;
}

void Account::setUploadLimit(const unsigned int limit)
{
    if (_uploadLimit == limit) {
        return;
    }

    _uploadLimit = limit;
    emit uploadLimitChanged();
}

unsigned int Account::downloadLimit() const
{
    return _downloadLimit;
}

void Account::setDownloadLimit(const unsigned int limit)
{
    if (_downloadLimit == limit) {
        return;
    }

    _downloadLimit = limit;
    emit downloadLimitChanged();
}

} // namespace OCC

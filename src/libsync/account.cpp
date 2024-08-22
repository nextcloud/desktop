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
#include "capabilities.h"
#include "common/asserts.h"
#include "cookiejar.h"
#include "creds/abstractcredentials.h"
#include "creds/credentialmanager.h"
#include "graphapi/spacesmanager.h"
#include "networkjobs.h"
#include "networkjobs/resources.h"
#include "theme.h"

#include <QAuthenticator>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QSettings>
#include <QSslKey>
#include <QSslSocket>
#include <QStandardPaths>

namespace OCC {

Q_LOGGING_CATEGORY(lcAccount, "sync.account", QtInfoMsg)

QString Account::_customCommonCacheDirectory = {};

void Account::setCommonCacheDirectory(const QString &directory)
{
    _customCommonCacheDirectory = directory;
}

QString Account::commonCacheDirectory()
{
    if (_customCommonCacheDirectory.isEmpty()) {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }

    return _customCommonCacheDirectory;
}

Account::Account(const QUuid &uuid, QObject *parent)
    : QObject(parent)
    , _uuid(uuid)
    , _capabilities({}, {})
    , _jobQueue(this)
    , _queueGuard(&_jobQueue)
    , _credentialManager(new CredentialManager(this))
{
    qRegisterMetaType<AccountPtr>("AccountPtr");

    _cacheDirectory = QStringLiteral("%1/accounts/%2").arg(commonCacheDirectory(), _uuid.toString(QUuid::WithoutBraces));
    QDir().mkpath(_cacheDirectory);

    // we need to make sure the directory we pass to the resources cache exists
    const QString resourcesCacheDir = QStringLiteral("%1/resources/").arg(_cacheDirectory);
    QDir().mkpath(resourcesCacheDir);
    _resourcesCache = new ResourcesCache(resourcesCacheDir, this);
}

AccountPtr Account::create(const QUuid &uuid)
{
    AccountPtr acc = AccountPtr(new Account(uuid));
    acc->setSharedThis(acc);
    return acc;
}

Account::~Account()
{
}

QString Account::davPath() const
{
    return QLatin1String("/remote.php/dav/files/") + davUser() + QLatin1Char('/');
}

void Account::setSharedThis(AccountPtr sharedThis)
{
    _sharedThis = sharedThis.toWeakRef();
}

CredentialManager *Account::credentialManager() const
{
    return _credentialManager;
}

QUuid Account::uuid() const
{
    return _uuid;
}

AccountPtr Account::sharedFromThis()
{
    return _sharedThis.toStrongRef();
}

QString Account::davUser() const
{
    return _davUser.isEmpty() ? _credentials->user() : _davUser;
}

void Account::setDavUser(const QString &newDavUser)
{
    if (_davUser == newDavUser)
        return;
    _davUser = newDavUser;
    Q_EMIT wantsAccountSaved(this);
}

QIcon Account::avatar() const
{
    if (_avatarImg.isNull()) {
        return Resources::getCoreIcon(QStringLiteral("account"));
    }
    return _avatarImg;
}

void Account::setAvatar(const QIcon &img)
{
    _avatarImg = img;
    Q_EMIT avatarChanged();
}

QString Account::displayNameWithHost() const
{
    QString user = davDisplayName();
    if (user.isEmpty())
        user = davUser();
    QString host = _url.host();
    const int port = url().port();
    if (port > 0 && port != 80 && port != 443) {
        host += QStringLiteral(":%1").arg(QString::number(port));
    }
    return tr("%1@%2").arg(user, host);
}

QString Account::davDisplayName() const
{
    return _displayName;
}

void Account::setDavDisplayName(const QString &newDisplayName)
{
    if (_displayName != newDisplayName) {
        _displayName = newDisplayName;
        Q_EMIT displayNameChanged();
    }
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
    if (_am) {
        jar = _am->cookieJar();
        jar->setParent(nullptr);
        _am->deleteLater();
    }

    // The order for these two is important! Reading the credential's
    // settings accesses the account as well as account->_credentials,
    _credentials.reset(cred);
    cred->setAccount(this);

    _am = _credentials->createAM();

    // the network access manager takes ownership when setCache is called, so we have to reinitialize it every time we reset the manager
    _networkCache = new QNetworkDiskCache(this);
    const QString networkCacheLocation = (QStringLiteral("%1/network/").arg(_cacheDirectory));
    qCDebug(lcAccount) << "Cache location for account" << this << "set to" << networkCacheLocation;
    _networkCache->setCacheDirectory(networkCacheLocation);
    _am->setCache(_networkCache);

    if (jar) {
        _am->setCookieJar(jar);
    }
    connect(_credentials.data(), &AbstractCredentials::fetched, this, [this] {
        Q_EMIT credentialsFetched();
        _queueGuard.unblock();
    });
    connect(_credentials.data(), &AbstractCredentials::authenticationStarted, this, [this] {
        _queueGuard.block();
    });
    connect(_credentials.data(), &AbstractCredentials::authenticationFailed, this, [this] { _queueGuard.clear(); });
}

QUrl Account::davUrl() const
{
    return Utility::concatUrlPath(url(), davPath());
}

/**
 * clear all cookies. (Session cookies or not)
 */
void Account::clearCookieJar()
{
    qCInfo(lcAccount) << "Clearing cookies";
    _am->cookieJar()->deleteLater();
    _am->setCookieJar(new CookieJar);
}

AccessManager *Account::accessManager()
{
    return _am.data();
}

QNetworkReply *Account::sendRawRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    Q_ASSERT(verb.isUpper());
    req.setUrl(url);
    if (verb == "HEAD" && !data) {
        return _am->head(req);
    } else if (verb == "GET" && !data) {
        return _am->get(req);
    } else if (verb == "POST") {
        return _am->post(req, data);
    } else if (verb == "PUT") {
        return _am->put(req, data);
    } else if (verb == "DELETE" && !data) {
        return _am->deleteResource(req);
    }
    return _am->sendCustomRequest(req, verb, data);
}

void Account::setApprovedCerts(const QList<QSslCertificate> &certs)
{
    _approvedCerts = { certs.begin(), certs.end() };
    _am->setCustomTrustedCaCertificates(_approvedCerts);
}

void Account::addApprovedCerts(const QSet<QSslCertificate> &certs)
{
    _approvedCerts.unite(certs);
    _am->setCustomTrustedCaCertificates(_approvedCerts);
    Q_EMIT wantsAccountSaved(this);
}

void Account::setUrl(const QUrl &url)
{
    _url = url;
}

QVariant Account::credentialSetting(const QString &key) const
{
    if (_credentials) {
        QString prefix = _credentials->authType();
        QVariant value = _settingsMap.value(prefix + QLatin1Char('_') + key);
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
        _settingsMap.insert(prefix + QLatin1Char('_') + key, value);
    }
}

JobQueue *Account::jobQueue()
{
    return &_jobQueue;
}

void Account::clearAMCache()
{
    _am->clearAccessCache();
}

const Capabilities &Account::capabilities() const
{
    Q_ASSERT(hasCapabilities());
    return _capabilities;
}

bool Account::hasCapabilities() const
{
    return _capabilities.isValid();
}

void Account::setCapabilities(const Capabilities &caps)
{
    const bool versionChanged = caps.status().legacyVersion != _capabilities.status().legacyVersion || caps.status().productversion != _capabilities.status().productversion;
    _capabilities = caps;
    if (versionChanged) {
        Q_EMIT serverVersionChanged();
    }
    if (!_spacesManager && _capabilities.spacesSupport().enabled) {
        _spacesManager = new GraphApi::SpacesManager(this);
    }
}

Account::ServerSupportLevel Account::serverSupportLevel() const
{
    if (!hasCapabilities()) {
        // not detected yet, assume it is fine.
        return ServerSupportLevel::Supported;
    }

    // ocis
    if (!capabilities().status().productversion.isEmpty()) {
        return ServerSupportLevel::Supported;
    }

    // Older version which is not "end of life" according to https://github.com/owncloud/core/wiki/Maintenance-and-Release-Schedule
    if (!capabilities().status().legacyVersion.isNull()) {
        if (capabilities().status().legacyVersion < QVersionNumber(10)) {
            return ServerSupportLevel::Unsupported;
        }
        return ServerSupportLevel::Supported;
    }
    return ServerSupportLevel::Unknown;
}

QString Account::defaultSyncRoot() const
{
    Q_ASSERT(!_defaultSyncRoot.isEmpty());
    return _defaultSyncRoot;
}
bool Account::hasDefaultSyncRoot() const
{
    return !_defaultSyncRoot.isEmpty();
}

void Account::setDefaultSyncRoot(const QString &syncRoot)
{
    Q_ASSERT(_defaultSyncRoot.isEmpty());
    if (!syncRoot.isEmpty()) {
        _defaultSyncRoot = syncRoot;
    }
}

void Account::setAppProvider(AppProvider &&p)
{
    _appProvider = std::move(p);
}

const AppProvider &Account::appProvider() const
{
    return _appProvider;
}

void Account::invalidCredentialsEncountered()
{
    Q_EMIT invalidCredentials(Account::QPrivateSignal());
}

ResourcesCache *Account::resourcesCache() const
{
    return _resourcesCache;
}

} // namespace OCC


QDebug operator<<(QDebug debug, const OCC::Account *acc)
{
    QDebugStateSaver saver(debug);
    debug.setAutoInsertSpaces(false);
    debug << "OCC::Account(" << acc->displayNameWithHost() << ")";
    return debug.maybeSpace();
}

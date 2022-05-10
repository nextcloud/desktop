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
#include "cookiejar.h"
#include "networkjobs.h"
#include "accessmanager.h"
#include "creds/abstractcredentials.h"
#include "creds/credentialmanager.h"
#include "capabilities.h"
#include "theme.h"
#include "common/asserts.h"

#include <QSettings>
#include <QLoggingCategory>
#include <QMutex>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QSslSocket>
#include <QNetworkCookieJar>
#include <QFileInfo>
#include <QDir>
#include <QSslKey>
#include <QAuthenticator>
#include <QStandardPaths>

namespace OCC {

Q_LOGGING_CATEGORY(lcAccount, "sync.account", QtInfoMsg)

Account::Account(QObject *parent)
    : QObject(parent)
    , _uuid(QUuid::createUuid())
    , _capabilities(QVariantMap())
    , _jobQueue(this)
    , _queueGuard(&_jobQueue)
    , _credentialManager(new CredentialManager(this))
{
    qRegisterMetaType<AccountPtr>("AccountPtr");
}

AccountPtr Account::create()
{
    AccountPtr acc = AccountPtr(new Account);
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
    emit wantsAccountSaved(this);
}

#ifndef TOKEN_AUTH_ONLY
QPixmap Account::avatar() const
{
    return _avatarImg;
}
void Account::setAvatar(const QPixmap &img)
{
    _avatarImg = img;
    emit accountChangedAvatar();
}
#endif

QString Account::displayName() const
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
    _displayName = newDisplayName;
    emit accountChangedDisplayName();
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

        _am.reset();
    }

    // The order for these two is important! Reading the credential's
    // settings accesses the account as well as account->_credentials,
    _credentials.reset(cred);
    cred->setAccount(this);

    // Note: This way the QNAM can outlive the Account and Credentials.
    // This is necessary to avoid issues with the QNAM being deleted while
    // processing slotHandleSslErrors().
    _am.reset(_credentials->createAM(), &QObject::deleteLater);

    if (jar) {
        _am->setCookieJar(jar);
    }
    connect(_am.data(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &Account::proxyAuthenticationRequired);
    connect(_credentials.data(), &AbstractCredentials::fetched,
        this, &Account::slotCredentialsFetched);
    connect(_credentials.data(), &AbstractCredentials::asked,
        this, &Account::slotCredentialsAsked);
    connect(_credentials.data(), &AbstractCredentials::authenticationStarted, this, [this] {
        _queueGuard.block();
    });
    connect(_credentials.data(), &AbstractCredentials::authenticationFailed, this, [this] {
        _queueGuard.clear();
    });
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
    auto jar = qobject_cast<CookieJar *>(_am->cookieJar());
    OC_ASSERT(jar);
    qCInfo(lcAccount) << "Clearing cookies";
    jar->setAllCookies(QList<QNetworkCookie>());
}

/*! This shares our official cookie jar (containing all the tasty
    authentication cookies) with another QNAM while making sure
    of not losing its ownership. */
void Account::lendCookieJarTo(QNetworkAccessManager *guest)
{
    auto jar = _am->cookieJar();
    auto oldParent = jar->parent();
    guest->setCookieJar(jar); // takes ownership of our precious cookie jar
    jar->setParent(oldParent); // takes it back
}

QString Account::cookieJarPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cookies") + id() + QStringLiteral(".db");
}

void Account::resetAccessManager()
{
    if (!_credentials || !_am) {
        return;
    }

    qCDebug(lcAccount) << "Resetting QNAM";
    QNetworkCookieJar *jar = _am->cookieJar();

    // Use a QSharedPointer to allow locking the life of the AM on the stack.
    // Make it call deleteLater to make sure that we can return to any AM stack frames safely.
    _am = QSharedPointer<AccessManager>(_credentials->createAM(), &QObject::deleteLater);
    _am->setCustomTrustedCaCertificates(approvedCerts());

    _am->setCookieJar(jar); // takes ownership of the old cookie jar
    connect(_am.data(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &Account::proxyAuthenticationRequired);
}

AccessManager *Account::accessManager()
{
    return _am.data();
}

QSharedPointer<AccessManager> Account::sharedAccessManager()
{
    return _am;
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

void Account::addApprovedCerts(const QList<QSslCertificate> &certs)
{
    _approvedCerts.unite({ certs.begin(), certs.end() });
    _am->setCustomTrustedCaCertificates(_approvedCerts);
}

void Account::resetRejectedCertificates()
{
    _rejectedCertificates.clear();
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

void Account::slotCredentialsFetched()
{
    emit credentialsFetched(_credentials.data());
    _queueGuard.unblock();
}

void Account::slotCredentialsAsked()
{
    emit credentialsAsked(_credentials.data());
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
    _capabilities = caps;
}

QString Account::serverVersion() const
{
    return _serverVersion;
}

int Account::serverVersionInt() const
{
    // FIXME: Use Qt 5.5 QVersionNumber
    auto components = serverVersion().split(QLatin1Char('.'));
    return makeServerVersion(components.value(0).toInt(),
        components.value(1).toInt(),
        components.value(2).toInt());
}

int Account::makeServerVersion(int majorVersion, int minorVersion, int patchVersion)
{
    return (majorVersion << 16) + (minorVersion << 8) + patchVersion;
}

bool Account::serverVersionUnsupported() const
{
    if (serverVersionInt() == 0) {
        // not detected yet, assume it is fine.
        return false;
    }
    // Older version which is not "end of life" according to https://github.com/owncloud/core/wiki/Maintenance-and-Release-Schedule
    return serverVersionInt() < makeServerVersion(10, 0, 0) || serverVersion().endsWith(QLatin1String("Nextcloud"));
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
        if (!QFileInfo::exists(syncRoot)) {
            OC_ASSERT(QDir().mkpath(syncRoot));
        }
    }
}

} // namespace OCC

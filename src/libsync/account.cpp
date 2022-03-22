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
    _am.reset(_credentials->createQNAM(), &QObject::deleteLater);

    if (jar) {
        _am->setCookieJar(jar);
    }
    connect(_am.data(), &QNetworkAccessManager::sslErrors,
        this, &Account::slotHandleSslErrors);
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
    emit wantsAccountSaved(this);
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

void Account::resetNetworkAccessManager()
{
    if (!_credentials || !_am) {
        return;
    }

    qCDebug(lcAccount) << "Resetting QNAM";
    QNetworkCookieJar *jar = _am->cookieJar();

    // Use a QSharedPointer to allow locking the life of the QNAM on the stack.
    // Make it call deleteLater to make sure that we can return to any QNAM stack frames safely.
    _am = QSharedPointer<QNetworkAccessManager>(_credentials->createQNAM(), &QObject::deleteLater);

    _am->setCookieJar(jar); // takes ownership of the old cookie jar
    connect(_am.data(), &QNetworkAccessManager::sslErrors, this,
        &Account::slotHandleSslErrors);
    connect(_am.data(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &Account::proxyAuthenticationRequired);
}

QNetworkAccessManager *Account::networkAccessManager()
{
    return _am.data();
}

QSharedPointer<QNetworkAccessManager> Account::sharedNetworkAccessManager()
{
    return _am;
}

QNetworkReply *Account::sendRawRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    Q_ASSERT(verb.isUpper());
    req.setUrl(url);
    req.setSslConfiguration(this->getOrCreateSslConfig());
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

    // Try hard to re-use session for different requests
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionTickets, false);
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionSharing, false);
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionPersistence, false);

    return sslConfig;
}

void Account::setApprovedCerts(const QList<QSslCertificate> certs)
{
    _approvedCerts = certs;
    QSslSocket::addDefaultCaCertificates(certs);
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

void Account::slotHandleSslErrors(QPointer<QNetworkReply> reply, const QList<QSslError> &errors)
{
    // Copy info out of reply ASAP
    if (reply.isNull()) {
        return;
    }

    const QString urlString = reply->url().toString();
    const auto sslConfiguration = reply->sslConfiguration();

    qCDebug(lcAccount) << "SSL diagnostics for url " << urlString;
    QList<QSslError> filteredErrors;
    QList<QSslError> ignoredErrors;
    for (const auto &error : qAsConst(errors)) {
        if (error.error() == QSslError::UnableToGetLocalIssuerCertificate) {
            // filter out this "error"
            qCDebug(lcAccount) << "- Info for " << error.certificate() << ": " << error.errorString()
                               << ". Local SSL certificates are known and always accepted.";
            ignoredErrors << error;
        } else {
            qCDebug(lcAccount) << "- Error for " << error.certificate() << ": "
                               << error.errorString() << "(" << int(error.error()) << ")"
                               << "\n";
            filteredErrors << error;
        }
    }

    // ask the _sslErrorHandler what to do with filteredErrors
    const auto handleErrors = [&urlString, &sslConfiguration, this](const QList<QSslError> &filteredErrors) -> QList<QSslError> {
        if (filteredErrors.isEmpty()) {
            return {};
        }
        bool allPreviouslyRejected = true;
        for (const auto &error : qAsConst(filteredErrors)) {
            if (!_rejectedCertificates.contains(error.certificate())) {
                allPreviouslyRejected = false;
                break;
            }
        }

        // If all certs have previously been rejected by the user, don't ask again.
        if (allPreviouslyRejected) {
            qCInfo(lcAccount) << "SSL diagnostics for url " << urlString
                              << ": certificates not trusted by user decision, returning.";
            return {};
        }

        if (_sslErrorHandler.isNull()) {
            qCWarning(lcAccount) << Q_FUNC_INFO << " called without a valid SSL error handler for account" << url()
                                 << "(" << urlString << ")";
            return {};
        }

        // SslDialogErrorHandler::handleErrors will run an event loop that might execute
        // the deleteLater() of the QNAM before we have the chance of unwinding our stack.
        // Keep a ref here on our stackframe to make sure that it doesn't get deleted before
        // handleErrors returns.
        QSharedPointer<QNetworkAccessManager> qnamLock = _am;
        QList<QSslCertificate> approvedCerts;
        if (_sslErrorHandler->handleErrors(filteredErrors, sslConfiguration, &approvedCerts, sharedFromThis())) {
            if (!approvedCerts.isEmpty()) {
                QSslSocket::addDefaultCaCertificates(approvedCerts);
                addApprovedCerts(approvedCerts);
                emit wantsAccountSaved(this);

                // all ssl certs are known and accepted. We can ignore the problems right away.
                qCDebug(lcAccount) << "Certs are known and trusted! This is not an actual error.";
            }
            return filteredErrors;
        } else {
            // Mark all involved certificates as rejected, so we don't ask the user again.
            for (const auto &error : qAsConst(filteredErrors)) {
                if (!_rejectedCertificates.contains(error.certificate())) {
                    _rejectedCertificates.append(error.certificate());
                }
            }
        }
        return {};
    };

    // Call `handleErrors` NOW, BEFORE checking if the scoped `reply` pointer. The lambda might take
    // a long time to complete: if a dialog is shown, the user could be "slow" to click it away, and
    // the reply might have been deleted at that point. So if we'd do this call inside the if below,
    // the object inside the reply guarded pointer could there, but might be gone *after* we finish
    // with `ignoreSslErrors`. Even when the scenario with a deleted reply happens, the handling is
    // still needed: the `handleErrors` call will also set the default CA certificates through
    // `QSslSocket::addDefaultCaCertificates()`, so future requests/replies can use those
    // user-approved certificates.
    auto moreIgnoredErrors = handleErrors(filteredErrors);

    // always apply the filter when we leave the scope
    if (reply) {
        // Warning: Do *not* use ignoreSslErrors() (without args) here:
        // it permanently ignores all SSL errors for this host, even
        // certificate changes.
        reply->ignoreSslErrors(ignoredErrors + moreIgnoredErrors);
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

void Account::clearQNAMCache()
{
    _am->clearAccessCache();
}

const Capabilities &Account::capabilities() const
{
    Q_ASSERT(_capabilities.isValid());
    return _capabilities;
}

void Account::setCapabilities(const QVariantMap &caps)
{
    _capabilities = Capabilities(caps);
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

} // namespace OCC

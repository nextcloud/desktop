/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "mirall/account.h"
#include "mirall/theme.h"
#include "mirall/mirallconfigfile.h"
#include "creds/abstractcredentials.h"
#include "creds/credentialsfactory.h"

#include <QSettings>
#include <QMutex>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QSslSocket>
#include <QNetworkCookieJar>

#include <QDebug>

namespace Mirall {

static const char urlC[] = "url";
static const char authTypeC[] = "authType";
static const char userC[] = "user";
static const char httpUserC[] = "http_user";

AccountManager *AccountManager::_instance = 0;

AccountManager *AccountManager::instance()
{
    static QMutex mutex;
    if (!_instance)
    {
        QMutexLocker lock(&mutex);
        if (!_instance) {
            _instance = new AccountManager;
        }
    }

    return _instance;
}

void AccountManager::setAccount(Account *account)
{
    emit accountAboutToChange(account, _account);
    std::swap(_account, account);
    emit accountChanged(_account, account);
}


Account::Account(AbstractSslErrorHandler *sslErrorHandler, QObject *parent)
    : QObject(parent)
    , _sslErrorHandler(sslErrorHandler)
    , _am(0)
    , _credentials(0)
    , _treatSslErrorsAsFailure(false)
    , _state(Account::Disconnected)
{
}

Account::~Account()
{
}

void Account::save()
{
    QScopedPointer<QSettings> settings(settingsWithGroup(Theme::instance()->appName()));
    settings->setValue(QLatin1String(urlC), _url.toString());
    if (_credentials) {
        _credentials->persist(this);
        Q_FOREACH(QString key, _settingsMap.keys()) {
            settings->setValue(key, _settingsMap.value(key));
        }
        settings->setValue(QLatin1String(authTypeC), _credentials->authType());

        // HACK: Save http_user also as user
        if (_settingsMap.contains(httpUserC))
            settings->setValue(userC, _settingsMap.value(httpUserC));
    }
    settings->sync();

    // ### TODO port away from MirallConfigFile
    MirallConfigFile cfg;
    qDebug() << "Saving " << approvedCerts().count() << " unknown certs.";
    QByteArray certs;
    Q_FOREACH( const QSslCertificate& cert, approvedCerts() ) {
        certs += cert.toPem() + '\n';
    }
    if (!certs.isEmpty()) {
        cfg.setCaCerts( certs );
    }
}

Account* Account::restore()
{
    QScopedPointer<QSettings> settings(settingsWithGroup(Theme::instance()->appName()));
    if (!settings->childKeys().isEmpty()) {
        Account *acc = new Account;
        MirallConfigFile cfg;
        acc->setApprovedCerts(QSslCertificate::fromData(cfg.caCerts()));
        acc->setUrl(settings->value(QLatin1String(urlC)).toUrl());
        acc->setCredentials(CredentialsFactory::create(settings->value(QLatin1String(authTypeC)).toString()));

        // We want to only restore settings for that auth type and the user value
        acc->_settingsMap.insert(QLatin1String(userC), settings->value(userC));
        QString authTypePrefix = settings->value(authTypeC).toString() + "_";
        Q_FOREACH(QString key, settings->childKeys()) {
            if (!key.startsWith(authTypePrefix))
                continue;
            acc->_settingsMap.insert(key, settings->value(key));
        }
        return acc;
    }
    return 0;
}

static bool isEqualExceptProtocol(const QUrl &url1, const QUrl &url2)
{
    return (url1.host() != url2.host() ||
            url1.port() != url2.port() ||
            url1.path() != url2.path());
}

bool Account::changed(Account *other, bool ignoreUrlProtocol) const
{
    if (!other) {
        return false;
    }
    bool changes = false;
    if (ignoreUrlProtocol) {
        changes = isEqualExceptProtocol(_url, other->_url);
    } else {
        changes = (_url == other->_url);
    }

    changes |= _credentials->changed(other->_credentials);

    return changes;
}

AbstractCredentials *Account::credentials() const
{
    return _credentials;
}

void Account::setCredentials(AbstractCredentials *cred)
{
    _credentials = cred;
    // set active credential manager
    if (_am) {
        _am->deleteLater();
    }
    _am = _credentials->getQNAM();
    connect(_am, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            SLOT(slotHandleErrors(QNetworkReply*,QList<QSslError>)));
}

QUrl Account::davUrl() const
{
    return concatUrlPath(url(), davPath());
}

QList<QNetworkCookie> Account::lastAuthCookies() const
{
    return _am->cookieJar()->cookiesForUrl(_url);
}

QNetworkReply *Account::headRequest(const QString &relPath)
{
    return headRequest(concatUrlPath(url(), relPath));
}

QNetworkReply *Account::headRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    return _am->head(request);
}

QNetworkReply *Account::getRequest(const QString &relPath)
{
    return getRequest(concatUrlPath(url(), relPath));
}

QNetworkReply *Account::getRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    return _am->get(request);
}

QNetworkReply *Account::davRequest(const QByteArray &verb, const QString &relPath, QNetworkRequest req, QIODevice *data)
{
    return davRequest(verb, concatUrlPath(davUrl(), relPath), req, data);
}

QNetworkReply *Account::davRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    req.setUrl(url);
    return _am->sendCustomRequest(req, verb, data);
}

void Account::setCertificateChain(const QList<QSslCertificate> &certs)
{
    _certificateChain = certs;
}

void Account::setApprovedCerts(const QList<QSslCertificate> certs)
{
    _approvedCerts = certs;
}

void Account::addApprovedCerts(const QList<QSslCertificate> certs)
{
    _approvedCerts += certs;
}

void Account::setSslErrorHandler(AbstractSslErrorHandler *handler)
{
    _sslErrorHandler.reset(handler);
}

void Account::setUrl(const QUrl &url)
{
    _url = url;
}

QUrl Account::concatUrlPath(const QUrl &url, const QString &concatPath)
{
    QUrl tmpUrl = url;
    QString path = tmpUrl.path();
    // avoid '//'
    if (path.endsWith('/') && concatPath.startsWith('/')) {
        path.chop(1);
    } // avoid missing '/'
    else if (!path.endsWith('/') && !concatPath.startsWith('/')) {
        path += QLatin1Char('/');
    }
    path += concatPath;
    tmpUrl.setPath(path);
    return tmpUrl;
}

QString Account::_configFileName;

QSettings *Account::settingsWithGroup(const QString& group)
{
    if (_configFileName.isEmpty()) {
        // cache file name
        MirallConfigFile cfg;
        _configFileName = cfg.configFile();
    }
    QSettings *settings = new QSettings(_configFileName, QSettings::IniFormat);
    settings->beginGroup(group);
    return settings;
}

QVariant Account::credentialSetting(const QString &key) const
{
    if (_credentials) {
        QString prefix = _credentials->authType();
        QString value = _settingsMap.value(prefix+"_"+key).toString();
        if (value.isEmpty()) {
            value = _settingsMap.value(key).toString();
        }
        return value;
    }
    return QVariant();
}

void Account::setCredentialSetting(const QString &key, const QVariant &value)
{
    if (_credentials) {
        QString prefix = _credentials->authType();
        _settingsMap.insert(prefix+"_"+key, value);
    }
}

int Account::state() const
{
    return _state;
}

void Account::setState(int state)
{
    if (_state != state) {
        _state = state;
        emit stateChanged(state);
    }
}

void Account::slotHandleErrors(QNetworkReply *reply , QList<QSslError> errors)
{
    qDebug() << "SSL-Warnings happened for url " << reply->url().toString();

    if( _treatSslErrorsAsFailure ) {
        // User decided once not to trust. Honor this decision.
        qDebug() << "Certs not trusted by user decision, returning.";
        return;
    }

    QList<QSslCertificate> approvedCerts;
    if (_sslErrorHandler.isNull() ) {
        qDebug() << Q_FUNC_INFO << "called without valid SSL error handler for account" << url();
    } else {
        if (_sslErrorHandler->handleErrors(errors, &approvedCerts, this)) {
            QSslSocket::addDefaultCaCertificates(approvedCerts);
            addApprovedCerts(approvedCerts);
            // all ssl certs are known and accepted. We can ignore the problems right away.
            qDebug() << "Certs are already known and trusted, Warnings are not valid.";
            reply->ignoreSslErrors();
        } else {
            _treatSslErrorsAsFailure = true;
            return;
        }
    }
}

} // namespace Mirall

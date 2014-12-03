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

#include "account.h"
#include "cookiejar.h"
#include "theme.h"
#include "networkjobs.h"
#include "configfile.h"
#include "accessmanager.h"
#include "quotainfo.h"
#include "owncloudtheme.h"
#include "creds/abstractcredentials.h"
#include "creds/credentialsfactory.h"

#include <QSettings>
#include <QMutex>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QSslSocket>
#include <QNetworkCookieJar>
#include <QFileInfo>
#include <QDir>

#include <QDebug>

namespace OCC {

static const char urlC[] = "url";
static const char authTypeC[] = "authType";
static const char userC[] = "user";
static const char httpUserC[] = "http_user";
static const char caCertsKeyC[] = "CaCertificates";

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
    , _url(Theme::instance()->overrideServerUrl())
    , _sslErrorHandler(sslErrorHandler)
    , _quotaInfo(new QuotaInfo(this))
    , _am(0)
    , _credentials(0)
    , _treatSslErrorsAsFailure(false)
    , _state(Account::Disconnected)
    , _davPath("remote.php/webdav/")
    , _wasMigrated(false)
{
    qRegisterMetaType<Account*>("Account*");
}

Account::~Account()
{
    delete _credentials;
    delete _am;
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

    // Save accepted certificates.
    settings->beginGroup(QLatin1String("General"));
    qDebug() << "Saving " << approvedCerts().count() << " unknown certs.";
    QByteArray certs;
    Q_FOREACH( const QSslCertificate& cert, approvedCerts() ) {
        certs += cert.toPem() + '\n';
    }
    if (!certs.isEmpty()) {
        settings->setValue( QLatin1String(caCertsKeyC), certs );
    }
}

Account* Account::restore()
{
    // try to open the correctly themed settings
    QScopedPointer<QSettings> settings(settingsWithGroup(Theme::instance()->appName()));

    Account *acc = 0;
    bool migratedCreds = false;

    // if the settings file could not be opened, the childKeys list is empty
    if( settings->childKeys().isEmpty() ) {
        // Now try to open the original ownCloud settings to see if they exist.
        QString oCCfgFile = QDir::fromNativeSeparators( settings->fileName() );
        // replace the last two segments with ownCloud/owncloud.cfg
        oCCfgFile = oCCfgFile.left( oCCfgFile.lastIndexOf('/'));
        oCCfgFile = oCCfgFile.left( oCCfgFile.lastIndexOf('/'));
        oCCfgFile += QLatin1String("/ownCloud/owncloud.cfg");

        qDebug() << "Migrate: checking old config " << oCCfgFile;

        QFileInfo fi( oCCfgFile );
        if( fi.isReadable() ) {
            QSettings *oCSettings = new QSettings(oCCfgFile, QSettings::IniFormat);
            oCSettings->beginGroup(QLatin1String("ownCloud"));

            // Check the theme url to see if it is the same url that the oC config was for
            QString overrideUrl = Theme::instance()->overrideServerUrl();
            if( !overrideUrl.isEmpty() ) {
                if (overrideUrl.endsWith('/')) { overrideUrl.chop(1); }
                QString oCUrl = oCSettings->value(QLatin1String(urlC)).toString();
                if (oCUrl.endsWith('/')) { oCUrl.chop(1); }

                // in case the urls are equal reset the settings object to read from
                // the ownCloud settings object
                qDebug() << "Migrate oC config if " << oCUrl << " == " << overrideUrl << ":"
                         << (oCUrl == overrideUrl ? "Yes" : "No");
                if( oCUrl == overrideUrl ) {
                    migratedCreds = true;
                    settings.reset( oCSettings );
                } else {
                    delete oCSettings;
                }
            }
        }
    }

    if (!settings->childKeys().isEmpty()) {
        acc = new Account;

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

        // now the cert, it is in the general group
        settings->beginGroup(QLatin1String("General"));
        acc->setApprovedCerts(QSslCertificate::fromData(settings->value(caCertsKeyC).toByteArray()));
        acc->setMigrated(migratedCreds);
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
    // set active credential manager
    QNetworkCookieJar *jar = 0;
    if (_am) {
        jar = _am->cookieJar();
        jar->setParent(0);

        _am->deleteLater();
    }

    if (_credentials) {
        credentials()->deleteLater();
    }
    _credentials = cred;
    _am = _credentials->getQNAM();
    if (jar) {
        _am->setCookieJar(jar);
    }
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

void Account::clearCookieJar()
{
    _am->setCookieJar(new CookieJar);
}

QNetworkAccessManager *Account::networkAccessManager()
{
    return _am;
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

void Account::setSslConfiguration(const QSslConfiguration &config)
{
    _sslConfiguration = config;
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

QSettings *Account::settingsWithGroup(const QString& group, QObject *parent)
{
    if (_configFileName.isEmpty()) {
        // cache file name
        ConfigFile cfg;
        _configFileName = cfg.configFile();
    }
    QSettings *settings = new QSettings(_configFileName, QSettings::IniFormat, parent);
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
        qDebug() << "Account state change: "
                 << stateString(_state) << "->" << stateString(state);
        _state = state;
        emit stateChanged(state);
    }
}

QString Account::stateString(int state)
{
    switch (state)
    {
    case Connected:
        return QLatin1String("Connected");
    case Disconnected:
        return QLatin1String("Disconnected");
    case SignedOut:
        return QLatin1String("SignedOut");
    case InvalidCredential:
        return QLatin1String("InvalidCredential");
    }
    return QLatin1String("Unknown");
}

QuotaInfo *Account::quotaInfo()
{
    return _quotaInfo;
}

void Account::slotHandleErrors(QNetworkReply *reply , QList<QSslError> errors)
{
    NetworkJobTimeoutPauser pauser(reply);
    qDebug() << "SSL-Errors happened for url " << reply->url().toString();
    foreach(const QSslError &error, errors) {
       qDebug() << "\tError in " << error.certificate() << ":"
                << error.errorString() << "("<< error.error()<< ")";
    }

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
            qDebug() << "Certs are already known and trusted, Errors are not valid.";
            reply->ignoreSslErrors();
        } else {
            _treatSslErrorsAsFailure = true;
            return;
        }
    }
}

bool Account::wasMigrated()
{
    return _wasMigrated;
}

void Account::setMigrated(bool mig)
{
    _wasMigrated = mig;
}

} // namespace OCC

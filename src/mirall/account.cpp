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

namespace Mirall {

static const char urlC[] = "url";
static const char userC[] = "user";
static const char authTypeC[] = "authType";

AccountManager *AccountManager::_instance = 0;

AccountManager *AccountManager::instance()
{
    static QMutex mutex;
    if (!_instance)
    {
      mutex.lock();

      if (!_instance) {
        _instance = new AccountManager;
      }
      mutex.unlock();
    }

    return _instance;
}


Account::Account(QObject *parent)
    : QObject(parent)
    , _am(0)
    , _credentials(0)
{
}

void Account::save(QSettings &settings)
{
    settings.beginGroup(Theme::instance()->appName());
    settings.setValue(QLatin1String(userC), _user);
    settings.setValue(QLatin1String(urlC), _url);
    if (_credentials) {
        settings.setValue(QLatin1String(authTypeC), _credentials->authType());
    }
}

Account* Account::restore(QSettings &settings)
{
    QString groupName = Theme::instance()->appName();
    if (settings.childGroups().contains(groupName)) {
        Account *acc = new Account;
        settings.beginGroup(groupName);
        acc->setUrl(settings.value(QLatin1String(urlC)).toUrl());
        acc->setUser(settings.value(QLatin1String(userC)).toString());
        MirallConfigFile cfg;
        acc->setCredentials(cfg.getCredentials());
        return acc;
    } else {
        return 0;
    }
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
}

static const char WEBDAV_PATH[] = "remote.php/webdav/";

QUrl Account::davUrl() const
{
    return concatUrlPath(url(), WEBDAV_PATH);
}

QList<QNetworkCookie> Account::lastAuthCookies() const
{
    return _am->cookieJar()->cookiesForUrl(_url);
}

QNetworkReply *Account::getRequest(const QString &relPath)
{
    QNetworkRequest request(concatUrlPath(url(), relPath));
    // ### error handling
    return _am->get(request);
}

QNetworkReply *Account::davRequest(const QByteArray &verb, const QString &relPath, QNetworkRequest req, QIODevice *data)
{
    req.setUrl(concatUrlPath(davUrl(), relPath));
    // ### error handling
    return _am->sendCustomRequest(req, verb, data);
}

void Account::setUrl(const QUrl &url)
{
    _url = url;
}

void Account::setUser(const QString &user)
{
    _user = user;
}

void Account::setCaCerts(const QByteArray &caCerts)
{
    _caCerts = caCerts;
}

QList<QSslCertificate> Account::certificateChain() const
{
    return QList<QSslCertificate>();
}

QUrl Account::concatUrlPath(const QUrl &url, const QString &concatPath)
{
    QUrl tmpUrl = url;
    QString path = tmpUrl.path();
    if (!path.endsWith('/')) {
        path += QLatin1Char('/');
    }
    path += concatPath;
    tmpUrl.setPath(path);
    return tmpUrl;
}

} // namespace Mirall

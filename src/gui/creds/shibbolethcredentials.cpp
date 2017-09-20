/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include <QSettings>
#include <QNetworkReply>
#include <QMessageBox>
#include <QAuthenticator>

#include "creds/shibbolethcredentials.h"
#include "creds/shibboleth/shibbolethwebview.h"
#include "creds/shibbolethcredentials.h"
#include "shibboleth/shibbolethuserjob.h"
#include "creds/credentialscommon.h"

#include "accessmanager.h"
#include "account.h"
#include "configfile.h"
#include "theme.h"
#include "cookiejar.h"
#include "owncloudgui.h"
#include "syncengine.h"

#include <keychain.h>

using namespace QKeychain;

namespace OCC {

Q_LOGGING_CATEGORY(lcShibboleth, "gui.credentials.shibboleth", QtInfoMsg)

namespace {

    // Not "user" because it has a special meaning for http
    const char userC[] = "shib_user";
    const char shibCookieNameC[] = "_shibsession_";

} // ns

ShibbolethCredentials::ShibbolethCredentials()
    : AbstractCredentials()
    , _url()
    , _ready(false)
    , _stillValid(false)
    , _browser(0)
    , _keychainMigration(false)
{
}

ShibbolethCredentials::ShibbolethCredentials(const QNetworkCookie &cookie)
    : _ready(true)
    , _stillValid(true)
    , _browser(0)
    , _shibCookie(cookie)
    , _keychainMigration(false)
{
}

void ShibbolethCredentials::setAccount(Account *account)
{
    AbstractCredentials::setAccount(account);

    // This is for existing saved accounts.
    if (_user.isEmpty()) {
        _user = _account->credentialSetting(QLatin1String(userC)).toString();
    }

    // When constructed with a cookie (by the wizard), we usually don't know the
    // user name yet. Request it now from the server.
    if (_ready && _user.isEmpty()) {
        QTimer::singleShot(1234, this, &ShibbolethCredentials::slotFetchUser);
    }
}

QString ShibbolethCredentials::authType() const
{
    return QString::fromLatin1("shibboleth");
}

QString ShibbolethCredentials::user() const
{
    return _user;
}

QNetworkAccessManager *ShibbolethCredentials::createQNAM() const
{
    QNetworkAccessManager *qnam(new AccessManager);
    connect(qnam, &QNetworkAccessManager::finished,
        this, &ShibbolethCredentials::slotReplyFinished);
    return qnam;
}

void ShibbolethCredentials::slotReplyFinished(QNetworkReply *r)
{
    if (!_browser.isNull()) {
        return;
    }

    QVariant target = r->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (target.isValid()) {
        _stillValid = false;
        // The Login window will be opened in NetworkJob's finished signal
        qCWarning(lcShibboleth) << "detected redirect, will open Login Window";
    } else {
        //_stillValid = true; // gets set when reading from keychain or getting it from browser
    }
}

bool ShibbolethCredentials::ready() const
{
    return _ready;
}

void ShibbolethCredentials::fetchFromKeychain()
{
    _wasFetched = true;

    if (_user.isEmpty()) {
        _user = _account->credentialSetting(QLatin1String(userC)).toString();
    }
    if (_ready) {
        Q_EMIT fetched();
    } else {
        _url = _account->url();
        _keychainMigration = false;
        fetchFromKeychainHelper();
    }
}

void ShibbolethCredentials::fetchFromKeychainHelper()
{
    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    job->setSettings(ConfigFile::settingsWithGroup(Theme::instance()->appName(), job).release());
    job->setInsecureFallback(false);
    job->setKey(keychainKey(_url.toString(), user(),
        _keychainMigration ? QString() : _account->id()));
    connect(job, &Job::finished, this, &ShibbolethCredentials::slotReadJobDone);
    job->start();
}

void ShibbolethCredentials::askFromUser()
{
    showLoginWindow();
}

bool ShibbolethCredentials::stillValid(QNetworkReply *reply)
{
    Q_UNUSED(reply)
    return _stillValid;
}

void ShibbolethCredentials::persist()
{
    storeShibCookie(_shibCookie);
    if (!_user.isEmpty()) {
        _account->setCredentialSetting(QLatin1String(userC), _user);
    }
}

void ShibbolethCredentials::invalidateToken()
{
    _ready = false;

    CookieJar *jar = static_cast<CookieJar *>(_account->networkAccessManager()->cookieJar());

    // Remove the _shibCookie
    auto cookies = jar->allCookies();
    for (auto it = cookies.begin(); it != cookies.end();) {
        if (it->name() == _shibCookie.name()) {
            it = cookies.erase(it);
        } else {
            ++it;
        }
    }
    jar->setAllCookies(cookies);

    // Clear all other temporary cookies
    jar->clearSessionCookies();
    removeShibCookie();
    _shibCookie = QNetworkCookie();
}

void ShibbolethCredentials::forgetSensitiveData()
{
    invalidateToken();
}

void ShibbolethCredentials::onShibbolethCookieReceived(const QNetworkCookie &shibCookie)
{
    storeShibCookie(shibCookie);
    _shibCookie = shibCookie;
    addToCookieJar(shibCookie);

    slotFetchUser();
}

void ShibbolethCredentials::slotFetchUser()
{
    // We must first do a request to webdav so the session is enabled.
    // (because for some reason we can't access the API without that..  a bug in the server maybe?)
    EntityExistsJob *job = new EntityExistsJob(_account->sharedFromThis(), _account->davPath(), this);
    connect(job, &EntityExistsJob::exists, this, &ShibbolethCredentials::slotFetchUserHelper);
    job->setIgnoreCredentialFailure(true);
    job->start();
}

void ShibbolethCredentials::slotFetchUserHelper()
{
    ShibbolethUserJob *job = new ShibbolethUserJob(_account->sharedFromThis(), this);
    connect(job, &ShibbolethUserJob::userFetched, this, &ShibbolethCredentials::slotUserFetched);
    job->start();
}

void ShibbolethCredentials::slotUserFetched(const QString &user)
{
    if (_user.isEmpty()) {
        if (user.isEmpty()) {
            qCWarning(lcShibboleth) << "Failed to fetch the shibboleth user";
        }
        _user = user;
    } else if (user != _user) {
        qCWarning(lcShibboleth) << "Wrong user: " << user << "!=" << _user;
        QMessageBox::warning(_browser, tr("Login Error"), tr("You must sign in as user %1").arg(_user));
        invalidateToken();
        showLoginWindow();
        return;
    }

    _stillValid = true;
    _ready = true;
    Q_EMIT asked();
}


void ShibbolethCredentials::slotBrowserRejected()
{
    _ready = false;
    Q_EMIT asked();
}

void ShibbolethCredentials::slotReadJobDone(QKeychain::Job *job)
{
    // If we can't find the credentials at the keys that include the account id,
    // try to read them from the legacy locations that don't have a account id.
    if (!_keychainMigration && job->error() == QKeychain::EntryNotFound) {
        qCWarning(lcShibboleth)
            << "Could not find keychain entry, attempting to read from legacy location";
        _keychainMigration = true;
        fetchFromKeychainHelper();
        return;
    }

    if (job->error() == QKeychain::NoError) {
        ReadPasswordJob *readJob = static_cast<ReadPasswordJob *>(job);
        delete readJob->settings();
        QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(readJob->textData().toUtf8());
        if (cookies.count() > 0) {
            _shibCookie = cookies.first();
            addToCookieJar(_shibCookie);
        }
        // access
        job->setSettings(ConfigFile::settingsWithGroup(Theme::instance()->appName(), job).release());

        _ready = true;
        _stillValid = true;
        Q_EMIT fetched();
    } else {
        _ready = false;
        Q_EMIT fetched();
    }


    // If keychain data was read from legacy location, wipe these entries and store new ones
    if (_keychainMigration && _ready) {
        persist();

        DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
        job->setSettings(ConfigFile::settingsWithGroup(Theme::instance()->appName(), job).release());
        job->setKey(keychainKey(_account->url().toString(), user(), QString()));
        job->start();

        qCWarning(lcShibboleth) << "Migrated old keychain entries";
    }
}

void ShibbolethCredentials::showLoginWindow()
{
    if (!_browser.isNull()) {
        ownCloudGui::raiseDialog(_browser);
        return;
    }

    CookieJar *jar = static_cast<CookieJar *>(_account->networkAccessManager()->cookieJar());
    // When opening a new window clear all the session cookie that might keep the user from logging in
    // (or the session may already be open in the server, and there will not be redirect asking for the
    // real long term cookie we want to store)
    jar->clearSessionCookies();

    _browser = new ShibbolethWebView(_account->sharedFromThis());
    connect(_browser.data(), &ShibbolethWebView::shibbolethCookieReceived,
        this, &ShibbolethCredentials::onShibbolethCookieReceived, Qt::QueuedConnection);
    connect(_browser.data(), &ShibbolethWebView::rejected, this, &ShibbolethCredentials::slotBrowserRejected);

    ownCloudGui::raiseDialog(_browser);
}

QList<QNetworkCookie> ShibbolethCredentials::accountCookies(Account *account)
{
    return account->networkAccessManager()->cookieJar()->cookiesForUrl(account->davUrl());
}

QNetworkCookie ShibbolethCredentials::findShibCookie(Account *account, QList<QNetworkCookie> cookies)
{
    if (cookies.isEmpty()) {
        cookies = accountCookies(account);
    }

    Q_FOREACH (QNetworkCookie cookie, cookies) {
        if (cookie.name().startsWith(shibCookieNameC)) {
            return cookie;
        }
    }
    return QNetworkCookie();
}

QByteArray ShibbolethCredentials::shibCookieName()
{
    return QByteArray(shibCookieNameC);
}

void ShibbolethCredentials::storeShibCookie(const QNetworkCookie &cookie)
{
    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    job->setSettings(ConfigFile::settingsWithGroup(Theme::instance()->appName(), job).release());
    // we don't really care if it works...
    //connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotWriteJobDone(QKeychain::Job*)));
    job->setKey(keychainKey(_account->url().toString(), user(), _account->id()));
    job->setTextData(QString::fromUtf8(cookie.toRawForm()));
    job->start();
}

void ShibbolethCredentials::removeShibCookie()
{
    DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
    job->setSettings(ConfigFile::settingsWithGroup(Theme::instance()->appName(), job).release());
    job->setKey(keychainKey(_account->url().toString(), user(), _account->id()));
    job->start();
}

void ShibbolethCredentials::addToCookieJar(const QNetworkCookie &cookie)
{
    QList<QNetworkCookie> cookies;
    cookies << cookie;
    QNetworkCookieJar *jar = _account->networkAccessManager()->cookieJar();
    jar->blockSignals(true); // otherwise we'd call ourselves
    jar->setCookiesFromUrl(cookies, _account->url());
    jar->blockSignals(false);
}

} // namespace OCC

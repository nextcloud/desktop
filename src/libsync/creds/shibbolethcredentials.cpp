/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include <QMutex>
#include <QSettings>
#include <QNetworkReply>
#include <QMessageBox>
#include <QAuthenticator>
#include <QDebug>

#include "creds/shibbolethcredentials.h"
#include "creds/shibboleth/shibbolethwebview.h"
#include "creds/shibboleth/shibbolethrefresher.h"
#include "creds/shibbolethcredentials.h"
#include "shibboleth/shibbolethuserjob.h"
#include "creds/credentialscommon.h"

#include "accessmanager.h"
#include "account.h"
#include "theme.h"
#include "cookiejar.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <qt5keychain/keychain.h>
#else
#include <qtkeychain/keychain.h>
#endif

using namespace QKeychain;

namespace OCC
{

namespace
{

// Not "user" because it has a special meaning for http
const char userC[] = "shib_user";
const char shibCookieNameC[] = "_shibsession_";

int shibboleth_redirect_callback(CSYNC* csync_ctx,
                                 const char* uri)
{
    if (!csync_ctx || !uri) {
        return 1;
    }

    const QString qurl(QString::fromLatin1(uri));
    QRegExp shibbolethyWords ("SAML|wayf");

    shibbolethyWords.setCaseSensitivity (Qt::CaseInsensitive);
    if (!qurl.contains(shibbolethyWords)) {
        return 1;
    }

    QMutex mutex;
    QMutexLocker locker(&mutex);
    Account *account = AccountManager::instance()->account();
    ShibbolethCredentials* creds = qobject_cast<ShibbolethCredentials*>(account->credentials());

    if (!creds) {
      qDebug() << "Not a Shibboleth creds instance!";
      return 1;
    }

    ShibbolethRefresher refresher(account, creds, csync_ctx);

    // blocks
    refresher.refresh();

    return creds->ready() ? 0 : 1;
}

} // ns

ShibbolethCredentials::ShibbolethCredentials()
    : AbstractCredentials(),
      _url(),
      _ready(false),
      _stillValid(false),
      _fetchJobInProgress(false),
      _browser(0)
{}

ShibbolethCredentials::ShibbolethCredentials(const QNetworkCookie& cookie, Account* account)
  : _ready(true),
    _stillValid(true),
    _fetchJobInProgress(false),
    _browser(0),
    _shibCookie(cookie)
{
    if (account) {
        /* The _user has not yet been fetched, so fetch it now */
        ShibbolethUserJob *job = new ShibbolethUserJob(account, this);
        connect(job, SIGNAL(userFetched(QString)), this, SLOT(slotUserFetched(QString)));
        QTimer::singleShot(1234, job, SLOT(start()));

    }
}


void ShibbolethCredentials::syncContextPreInit(CSYNC* ctx)
{
    csync_set_auth_callback (ctx, handleNeonSSLProblems);
}

QByteArray ShibbolethCredentials::prepareCookieData() const
{
    QString cookiesAsString;
    // TODO: This should not be a part of this method, but we don't
    // have any way to get "session_key" module property from
    // csync. Had we have it, then we could just append shibboleth
    // cookies to the "session_key" value and set it in csync module.
    Account *account = AccountManager::instance()->account();
    QList<QNetworkCookie> cookies = accountCookies(account);

    foreach(const QNetworkCookie &cookie, cookies) {
        cookiesAsString  += cookie.toRawForm(QNetworkCookie::NameAndValueOnly) + QLatin1String("; ");
    }

    return cookiesAsString.toLatin1();
}

void ShibbolethCredentials::syncContextPreStart (CSYNC* ctx)
{
    typedef int (*csync_owncloud_redirect_callback_t)(CSYNC* ctx, const char* uri);

    csync_owncloud_redirect_callback_t cb = shibboleth_redirect_callback;

    csync_set_module_property(ctx, "session_key", prepareCookieData().data());
    csync_set_module_property(ctx, "redirect_callback", &cb);
}

bool ShibbolethCredentials::changed(AbstractCredentials* credentials) const
{
    ShibbolethCredentials* other(qobject_cast< ShibbolethCredentials* >(credentials));

    if (!other) {
        return true;
    }

    if (_shibCookie != other->_shibCookie || _user != other->_user) {
        return true;
    }

    return false;
}

QString ShibbolethCredentials::authType() const
{
    return QString::fromLatin1("shibboleth");
}

QString ShibbolethCredentials::user() const
{
    return _user;
}

QNetworkAccessManager* ShibbolethCredentials::getQNAM() const
{
    QNetworkAccessManager* qnam(new AccessManager);
    connect(qnam, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(slotReplyFinished(QNetworkReply*)));
    return qnam;
}

void ShibbolethCredentials::slotReplyFinished(QNetworkReply* r)
{
    if (!_browser.isNull()) {
        return;
    }

    QVariant target = r->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (target.isValid()) {
        _stillValid = false;
        qWarning() << Q_FUNC_INFO << "detected redirect, will open Login Window"; // will be done in NetworkJob's finished signal
    } else {
        //_stillValid = true; // gets set when reading from keychain or getting it from browser
    }
}

bool ShibbolethCredentials::ready() const
{
    return _ready;
}

void ShibbolethCredentials::fetch(Account *account)
{

    if(_fetchJobInProgress) {
        return;
    }

    if (_user.isEmpty()) {
        _user = account->credentialSetting(QLatin1String(userC)).toString();
    }
    if (_ready) {
        _fetchJobInProgress = false;
        Q_EMIT fetched();
    } else {
        if (account) {
            _url = account->url();
        }
        ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
        job->setSettings(account->settingsWithGroup(Theme::instance()->appName(), job));
        job->setInsecureFallback(false);
        job->setKey(keychainKey(account->url().toString(), "shibAssertion"));
        job->setProperty("account", QVariant::fromValue(account));
        connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotReadJobDone(QKeychain::Job*)));
        job->start();
        _fetchJobInProgress = true;
    }
}

bool ShibbolethCredentials::stillValid(QNetworkReply *reply)
{
    Q_UNUSED(reply)
    return _stillValid;
}

void ShibbolethCredentials::persist(Account* account)
{
    storeShibCookie(_shibCookie, account);
    if (!_user.isEmpty()) {
        account->setCredentialSetting(QLatin1String(userC), _user);
    }
}

// only used by Application::slotLogout(). Use invalidateAndFetch for normal usage
void ShibbolethCredentials::invalidateToken(Account *account)
{
    CookieJar *jar = static_cast<CookieJar*>(account->networkAccessManager()->cookieJar());

    // Remove the _shibCookie
    auto cookies = jar->allCookies();
    for (auto it = cookies.begin(); it != cookies.end(); ) {
        if (it->name() == _shibCookie.name()) {
            it = cookies.erase(it);
        } else {
            ++it;
        }
    }
    jar->setAllCookies(cookies);

    // Clear all other temporary cookies
    jar->clearSessionCookies();
    removeShibCookie(account);
    _shibCookie = QNetworkCookie();
    // ### access to ctx missing, but might not be required at all
    //csync_set_module_property(ctx, "session_key", "");
}

void ShibbolethCredentials::onShibbolethCookieReceived(const QNetworkCookie& shibCookie, Account *account)
{
    storeShibCookie(shibCookie, account);
    _shibCookie = shibCookie;
    addToCookieJar(shibCookie);

    // Now fetch the user...
    // But we must first do a request to webdav so the session is enabled.
    // (because for some reason we wan't access the API without that..  a bug in the server maybe?)
    EntityExistsJob* job = new EntityExistsJob(account, account->davPath(), this);
    connect(job, SIGNAL(exists(QNetworkReply*)), this, SLOT(slotFetchUser()));
    job->setIgnoreCredentialFailure(true);
    job->start();
}

void ShibbolethCredentials::slotFetchUser()
{
    AbstractNetworkJob* oldjob = qobject_cast<AbstractNetworkJob*>(sender());
    Q_ASSERT(oldjob);
    ShibbolethUserJob *job = new ShibbolethUserJob(oldjob->account(), this);
    connect(job, SIGNAL(userFetched(QString)), this, SLOT(slotUserFetched(QString)));
    job->start();
}


void ShibbolethCredentials::slotUserFetched(const QString &user)
{
    ShibbolethUserJob *job = qobject_cast<ShibbolethUserJob *>(sender());
    Q_ASSERT(job);
    if (_user.isEmpty()) {
        _user = user;
    } else if (user != _user) {
        qDebug() << "Wrong user: " << user << "!=" << _user;
        QMessageBox::warning(_browser, tr("Login Error"), tr("You must sign in as user %1").arg(_user));
        invalidateToken(job->account());
        showLoginWindow(job->account());
        return;
    }

    _stillValid = true;
    _ready = true;
    _fetchJobInProgress = false;
    Q_EMIT fetched();
}


void ShibbolethCredentials::slotBrowserRejected()
{
    _ready = false;
    _fetchJobInProgress = false;
    Q_EMIT fetched();
}

void ShibbolethCredentials::invalidateAndFetch(Account* account)
{
    _ready = false;
    _fetchJobInProgress = true;

    // delete the credentials, then in the slot fetch them again (which will trigger browser)
    DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
    job->setProperty("account", QVariant::fromValue(account));
    job->setSettings(account->settingsWithGroup(Theme::instance()->appName(), job));
    connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotInvalidateAndFetchInvalidateDone(QKeychain::Job*)));
    job->setKey(keychainKey(account->url().toString(), "shibAssertion"));
    job->start();
}

void ShibbolethCredentials::slotInvalidateAndFetchInvalidateDone(QKeychain::Job* job)
{
    Account *account = qvariant_cast<Account*>(job->property("account"));

    connect (this, SIGNAL(fetched()),
             this, SLOT(onFetched()));
    _fetchJobInProgress = false;
    // small hack to support the ShibbolethRefresher hack
    // we already rand fetch() with a valid account object,
    // and hence know the url on refresh
    fetch(account);
}

void ShibbolethCredentials::onFetched()
{
    disconnect (this, SIGNAL(fetched()),
                this, SLOT(onFetched()));

    Q_EMIT invalidatedAndFetched(prepareCookieData());
}

void ShibbolethCredentials::slotReadJobDone(QKeychain::Job *job)
{
    Account *account = qvariant_cast<Account*>(job->property("account"));
    if (job->error() == QKeychain::NoError) {
        ReadPasswordJob *readJob = static_cast<ReadPasswordJob*>(job);
        delete readJob->settings();
        QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(readJob->textData().toUtf8());
        if (cookies.count() > 0) {
            _shibCookie = cookies.first();
            addToCookieJar(_shibCookie);
        }
        // access
        job->setSettings(account->settingsWithGroup(Theme::instance()->appName(), job));

        _ready = true;
        _stillValid = true;
        _fetchJobInProgress = false;
        Q_EMIT fetched();
    } else {
        showLoginWindow(account);
    }
}

void ShibbolethCredentials::showLoginWindow(Account* account)
{
    if (!_browser.isNull()) {
        _browser->activateWindow();
        _browser->raise();
        // FIXME On OS X this does not raise properly
        return;
    }

    CookieJar *jar = static_cast<CookieJar*>(account->networkAccessManager()->cookieJar());
    // When opening a new window clear all the session cookie that might keep the user from logging in
    // (or the session may already be open in the server, and there will not be redirect asking for the
    // real long term cookie we want to store)
    jar->clearSessionCookies();

    _browser = new ShibbolethWebView(account);
    connect(_browser, SIGNAL(shibbolethCookieReceived(QNetworkCookie, Account*)),
            this, SLOT(onShibbolethCookieReceived(QNetworkCookie, Account*)), Qt::QueuedConnection);
    connect(_browser, SIGNAL(rejected()), this, SLOT(slotBrowserRejected()));

    _browser->show();
}

QList<QNetworkCookie> ShibbolethCredentials::accountCookies(Account *account)
{
    return account->networkAccessManager()->cookieJar()->cookiesForUrl(account->davUrl());
}

QNetworkCookie ShibbolethCredentials::findShibCookie(Account *account, QList<QNetworkCookie> cookies)
{
    if(cookies.isEmpty()) {
        cookies = accountCookies(account);
    }

    Q_FOREACH(QNetworkCookie cookie, cookies) {
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

void ShibbolethCredentials::storeShibCookie(const QNetworkCookie &cookie, Account *account)
{
    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    job->setSettings(account->settingsWithGroup(Theme::instance()->appName(), job));
    // we don't really care if it works...
    //connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotWriteJobDone(QKeychain::Job*)));
    job->setKey(keychainKey(account->url().toString(), "shibAssertion"));
    job->setTextData(QString::fromUtf8(cookie.toRawForm()));
    job->start();
}

void ShibbolethCredentials::removeShibCookie(Account *account)
{
    DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
    job->setSettings(account->settingsWithGroup(Theme::instance()->appName(), job));
    job->setKey(keychainKey(account->url().toString(), "shibAssertion"));
    job->start();
}

void ShibbolethCredentials::addToCookieJar(const QNetworkCookie &cookie)
{
    QList<QNetworkCookie> cookies;
    cookies << cookie;
    Account *account = AccountManager::instance()->account();
    QNetworkCookieJar *jar = account->networkAccessManager()->cookieJar();
    jar->blockSignals(true); // otherwise we'd call ourselves
    jar->setCookiesFromUrl(cookies, account->url());
    jar->blockSignals(false);
}


} // namespace OCC

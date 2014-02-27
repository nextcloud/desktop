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

#include "creds/shibbolethcredentials.h"
#include "creds/shibboleth/shibbolethaccessmanager.h"
#include "creds/shibboleth/shibbolethwebview.h"
#include "creds/shibboleth/shibbolethrefresher.h"
#include "creds/shibboleth/shibbolethconfigfile.h"
#include "creds/credentialscommon.h"

#include "mirall/account.h"
#include "mirall/theme.h"

#include <qtkeychain/keychain.h>

using namespace QKeychain;

namespace Mirall
{

namespace
{

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
      _shibCookie(),
      _ready(false),
      _stillValid(false),
      _browser(0),
      _otherCookies()
{}

ShibbolethCredentials::ShibbolethCredentials(const QNetworkCookie& cookie, const QMap<QUrl, QList<QNetworkCookie> >& otherCookies)
    : _shibCookie(cookie),
      _ready(true),
      _browser(0),
      _otherCookies(otherCookies)
{}

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
    QList<QNetworkCookie> cookies(AccountManager::instance()->account()->lastAuthCookies());
    QMap<QString, QString> uniqueCookies;

    cookies << _shibCookie;
    // Stuff cookies inside csync, then we can avoid the intermediate HTTP 401 reply
    // when https://github.com/owncloud/core/pull/4042 is merged.
    foreach(QNetworkCookie c, cookies) {
        const QString cookieName(c.name());

        if (cookieName.startsWith("_shibsession_")) {
            continue;
        }
        uniqueCookies.insert(cookieName, c.value());
    }

    if (!_shibCookie.name().isEmpty()) {
        uniqueCookies.insert(_shibCookie.name(), _shibCookie.value());
    }
    foreach(const QString& cookieName, uniqueCookies.keys()) {
        cookiesAsString += cookieName;
        cookiesAsString += '=';
        cookiesAsString += uniqueCookies[cookieName];
        cookiesAsString += "; ";
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
    ShibbolethCredentials* other(dynamic_cast< ShibbolethCredentials* >(credentials));

    if (!other || other->cookie() != this->cookie()) {
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
    // ### TODO: If we had a way to extract the currently authenticated user
    // somehow, we could return its id token (email) here (stored in REMOTE_USER)
    // The server doesn't return it by default
    return QString();
}

QNetworkCookie ShibbolethCredentials::cookie() const
{
    return _shibCookie;
}

QNetworkAccessManager* ShibbolethCredentials::getQNAM() const
{
    ShibbolethAccessManager* qnam(new ShibbolethAccessManager(_shibCookie));

    connect(this, SIGNAL(newCookie(QNetworkCookie)),
            qnam, SLOT(setCookie(QNetworkCookie)));
    connect(qnam, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(slotReplyFinished(QNetworkReply*)));
    return qnam;
}

void ShibbolethCredentials::slotReplyFinished(QNetworkReply* r)
{
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
    if (_ready) {
        Q_EMIT fetched();
    } else {
        if (account) {
            _url = account->url();
        }
        ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
        job->setSettings(account->settingsWithGroup(Theme::instance()->appName()));
        job->setInsecureFallback(false);
        job->setKey(keychainKey(account->url().toString(), "shibAssertion"));
        job->setProperty("account", QVariant::fromValue(account));
        connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotReadJobDone(QKeychain::Job*)));
        job->start();
    }
}

bool ShibbolethCredentials::stillValid(QNetworkReply *reply)
{
    Q_UNUSED(reply)
    return _stillValid;
}

void ShibbolethCredentials::persist(Account* account)
{
    ShibbolethConfigFile cfg;

    cfg.storeCookies(_otherCookies);

    storeShibCookie(_shibCookie, account);
}

// only used by Application::slotLogout(). Use invalidateAndFetch for normal usage
void ShibbolethCredentials::invalidateToken(Account *account)
{
    Q_UNUSED(account)
    _shibCookie = QNetworkCookie();
    storeShibCookie(_shibCookie, account); // store/erase cookie

    // ### access to ctx missing, but might not be required at all
    //csync_set_module_property(ctx, "session_key", "");
}

void ShibbolethCredentials::disposeBrowser()
{
    qDebug() << Q_FUNC_INFO;
    disconnect(_browser, SIGNAL(viewHidden()),
               this, SLOT(slotBrowserHidden()));
    disconnect(_browser, SIGNAL(shibbolethCookieReceived(QNetworkCookie, Account*)),
               this, SLOT(onShibbolethCookieReceived(QNetworkCookie, Account*)));
    _browser->hide();
    _browser->deleteLater();
    _browser = 0;
}

void ShibbolethCredentials::onShibbolethCookieReceived(const QNetworkCookie& cookie, Account* account)
{
    disposeBrowser();
    _ready = true;
    _stillValid = true;
    _shibCookie = cookie;
    storeShibCookie(_shibCookie, account);
    Q_EMIT newCookie(_shibCookie);
    Q_EMIT fetched();
}

void ShibbolethCredentials::slotBrowserHidden()
{
    disposeBrowser();
    _ready = false;
    _shibCookie = QNetworkCookie();
    Q_EMIT fetched();
}

void ShibbolethCredentials::invalidateAndFetch(Account* account)
{
    _ready = false;

    // delete the credentials, then in the slot fetch them again (which will trigger browser)
    DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
    job->setProperty("account", QVariant::fromValue(account));
    job->setSettings(account->settingsWithGroup(Theme::instance()->appName()));
    connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotInvalidateAndFetchInvalidateDone(QKeychain::Job*)));
    job->setKey(keychainKey(account->url().toString(), "shibAssertion"));
    job->start();
}

void ShibbolethCredentials::slotInvalidateAndFetchInvalidateDone(QKeychain::Job* job)
{
    Account *account = qvariant_cast<Account*>(job->property("account"));

    connect (this, SIGNAL(fetched()),
             this, SLOT(onFetched()));
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
        qDebug() << Q_FUNC_INFO;
        QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(readJob->textData().toUtf8());
        if (cookies.count() > 0) {
            _shibCookie = cookies.first();
        }
        job->setSettings(account->settingsWithGroup(Theme::instance()->appName()));

        _ready = true;
        _stillValid = true;
        Q_EMIT newCookie(_shibCookie);
        Q_EMIT fetched();
    } else {
        showLoginWindow(account);
    }
}

void ShibbolethCredentials::showLoginWindow(Account* account)
{
    if (_browser) {
        _browser->activateWindow();
        _browser->raise();
        // FIXME On OS X this does not raise properly
        return;
    }
    ShibbolethConfigFile cfg;
    _browser = new ShibbolethWebView(account, cfg.createCookieJar());
    connect(_browser, SIGNAL(shibbolethCookieReceived(QNetworkCookie, Account*)),
            this, SLOT(onShibbolethCookieReceived(QNetworkCookie, Account*)));
    connect(_browser, SIGNAL(viewHidden()),
            this, SLOT(slotBrowserHidden()));
    // FIXME If the browser was hidden (e.g. user closed it) without us logging in, the logic gets stuck
    // and can only be unstuck by restarting the app or pressing "Sign in" (we should switch to offline but we don't)

    _browser->show();
}

void ShibbolethCredentials::storeShibCookie(const QNetworkCookie &cookie, Account *account)
{
    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    job->setSettings(account->settingsWithGroup(Theme::instance()->appName()));
    // we don't really care if it works...
    //connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotWriteJobDone(QKeychain::Job*)));
    job->setKey(keychainKey(account->url().toString(), "shibAssertion"));
    job->setTextData(QString::fromUtf8(cookie.toRawForm()));
    job->start();
}

} // ns Mirall

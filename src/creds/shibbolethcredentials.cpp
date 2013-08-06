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

#include "creds/shibbolethcredentials.h"
#include "creds/shibboleth/shibbolethaccessmanager.h"
#include "creds/shibboleth/shibbolethwebview.h"
#include "creds/shibboleth/shibbolethrefresher.h"
#include "creds/credentialscommon.h"
#include "mirall/owncloudinfo.h"
#include "mirall/mirallconfigfile.h"

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
    MirallConfigFile cfg;
    ShibbolethCredentials* creds = dynamic_cast< ShibbolethCredentials* > (cfg.getCredentials());

    if (!creds) {
      qDebug() << "Not a Shibboleth creds instance!";
      return 1;
    }

    ShibbolethRefresher refresher(creds, csync_ctx);

    // blocks
    refresher.refresh();

    return 0;
}

} // ns

ShibbolethCredentials::ShibbolethCredentials()
    : _shibCookie(),
      _ready(false),
      _browser(0)
{}

ShibbolethCredentials::ShibbolethCredentials(const QNetworkCookie& cookie)
    : _shibCookie(cookie),
      _ready(true),
      _browser(0)
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
    QList<QNetworkCookie> cookies(ownCloudInfo::instance()->getLastAuthCookies());
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

    uniqueCookies.insert(_shibCookie.name(), _shibCookie.value());
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

QNetworkCookie ShibbolethCredentials::cookie() const
{
    return _shibCookie;
}

QNetworkAccessManager* ShibbolethCredentials::getQNAM() const
{
    ShibbolethAccessManager* qnam(new ShibbolethAccessManager(_shibCookie));

    connect(this, SIGNAL(newCookie(QNetworkCookie)),
            qnam, SLOT(setCookie(QNetworkCookie)));
    return qnam;
}

bool ShibbolethCredentials::ready() const
{
    return _ready;
}

void ShibbolethCredentials::fetch()
{
    if (_ready) {
        Q_EMIT fetched();
    } else {
        MirallConfigFile cfg;

        _browser = new ShibbolethWebView(QUrl(cfg.ownCloudUrl()));
        connect(_browser, SIGNAL(shibbolethCookieReceived(QNetworkCookie)),
                this, SLOT(onShibbolethCookieReceived(QNetworkCookie)));
        _browser->show ();
    }
}

void ShibbolethCredentials::persistForUrl(const QString& /*url*/)
{
    // nothing to do here, we don't store session cookies.
}

void ShibbolethCredentials::onShibbolethCookieReceived(const QNetworkCookie& cookie)
{
    _browser->hide();
    disconnect(_browser, SIGNAL(shibbolethCookieReceived(QNetworkCookie)),
               this, SLOT(onShibbolethCookieReceived(QNetworkCookie)));
    _browser->deleteLater();
    _browser = 0;
    _ready = true;
    _shibCookie = cookie;
    Q_EMIT newCookie(_shibCookie);
    Q_EMIT fetched();
}

void ShibbolethCredentials::invalidateAndFetch()
{
    _ready = false;
    connect (this, SIGNAL(fetched()),
             this, SLOT(onFetched()));
    fetch();
}

void ShibbolethCredentials::onFetched()
{
    disconnect (this, SIGNAL(fetched()),
                this, SLOT(onFetched()));

    Q_EMIT invalidatedAndFetched(prepareCookieData());
}

} // ns Mirall

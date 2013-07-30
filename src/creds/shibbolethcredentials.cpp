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
#include "mirall/owncloudinfo.h"
#include "mirall/mirallconfigfile.h"

namespace Mirall
{

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

void ShibbolethCredentials::syncContextPreInit (CSYNC*)
{}

void ShibbolethCredentials::syncContextPreStart (CSYNC* ctx)
{
  QString cookiesAsString;
  // TODO: This should not be a part of this method, but we don't
  // have any way to get "session_key" module property from
  // csync. Had we have it, then we could just append shibboleth
  // cookies to the "session_key" value and set it in csync module.
  QList<QNetworkCookie> cookies(ownCloudInfo::instance()->getLastAuthCookies());

  cookies << _shibCookie;
  // Stuff cookies inside csync, then we can avoid the intermediate HTTP 401 reply
  // when https://github.com/owncloud/core/pull/4042 is merged.
  foreach(QNetworkCookie c, cookies) {
    cookiesAsString += c.name();
    cookiesAsString += '=';
    cookiesAsString += c.value();
    cookiesAsString += "; ";
  }

  csync_set_module_property(ctx, "session_key", cookiesAsString.toLatin1().data());
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

} // ns Mirall

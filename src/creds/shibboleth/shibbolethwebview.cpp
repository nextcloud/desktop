/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include <QDebug>
#include <QNetworkCookie>
#include <QWebFrame>
#include <QWebPage>

#include "creds/shibboleth/shibbolethcookiejar.h"
#include "creds/shibboleth/shibbolethwebview.h"
#include "mirall/mirallaccessmanager.h"

namespace Mirall
{

void ShibbolethWebView::setup(const QUrl& url, ShibbolethCookieJar* jar)
{
    MirallAccessManager* nm = new MirallAccessManager(this);
    QWebPage* page = new QWebPage(this);

    jar->setParent(this);
    connect (jar, SIGNAL (newCookiesForUrl (QList<QNetworkCookie>, QUrl)),
             this, SLOT (onNewCookiesForUrl (QList<QNetworkCookie>, QUrl)));

    nm->setCookieJar(jar);
    page->setNetworkAccessManager(nm);
    page->mainFrame ()->load (url);
    this->setPage (page);
}

ShibbolethWebView::ShibbolethWebView(const QUrl& url, QWidget* parent)
  : QWebView(parent)
{
    setup(url, new ShibbolethCookieJar(this));
}

ShibbolethWebView::ShibbolethWebView(const QUrl& url, ShibbolethCookieJar* jar, QWidget* parent)
  : QWebView(parent)
{
    setup(url, jar);
}

void ShibbolethWebView::onNewCookiesForUrl (const QList<QNetworkCookie>& cookieList, const QUrl& url)
{
  QList<QNetworkCookie> otherCookies;
  QNetworkCookie shibCookie;

  Q_FOREACH (const QNetworkCookie& cookie, cookieList) {
    if (cookie.name().startsWith ("_shibsession_")) {
      if (shibCookie.name().isEmpty()) {
        shibCookie = cookie;
      } else {
        qWarning() << "Too many Shibboleth session cookies at once!";
      }
    } else {
      otherCookies << cookie;
    }
  }

  if (!otherCookies.isEmpty()) {
    Q_EMIT otherCookiesReceived(otherCookies, url);
  }
  if (!shibCookie.name().isEmpty()) {
    Q_EMIT shibbolethCookieReceived(shibCookie);
  }
}

void ShibbolethWebView::hideEvent(QHideEvent* event)
{
    Q_EMIT viewHidden();
    QWebView::hideEvent(event);
}

} // ns Mirall

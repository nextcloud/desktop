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

#include <QApplication>
#include <QDebug>
#include <QNetworkCookie>
#include <QWebFrame>
#include <QWebPage>
#include <QMessageBox>

#include "creds/shibboleth/shibbolethcookiejar.h"
#include "creds/shibboleth/shibbolethwebview.h"
#include "mirall/account.h"
#include "mirall/mirallaccessmanager.h"
#include "mirall/theme.h"

namespace Mirall
{

void ShibbolethWebView::setup(Account *account, ShibbolethCookieJar* jar)
{
    MirallAccessManager* nm = new MirallAccessManager(this);
    // we need our own QNAM, but the we offload the SSL error handling to
    // the account object, which already can do this
    connect(nm, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            account, SLOT(slotHandleErrors(QNetworkReply*,QList<QSslError>)));

    QWebPage* page = new QWebPage(this);

    jar->setParent(this);
    connect(jar, SIGNAL (newCookiesForUrl (QList<QNetworkCookie>, QUrl)),
            this, SLOT (onNewCookiesForUrl (QList<QNetworkCookie>, QUrl)));
    connect(page, SIGNAL(loadStarted()),
            this, SLOT(slotLoadStarted()));
    connect(page, SIGNAL(loadFinished(bool)),
            this, SLOT(slotLoadFinished(bool)));

    nm->setCookieJar(jar);
    page->setNetworkAccessManager(nm);
    page->mainFrame()->load(account->url());
    this->setPage(page);
    setWindowTitle(tr("%1 - Authenticate").arg(Theme::instance()->appNameGUI()));
}

ShibbolethWebView::ShibbolethWebView(Account* account, QWidget* parent)
  : QWebView(parent)
{
    setup(account, new ShibbolethCookieJar(this));
}

ShibbolethWebView::~ShibbolethWebView()
{
    slotLoadFinished();
}

ShibbolethWebView::ShibbolethWebView(Account* account, ShibbolethCookieJar* jar, QWidget* parent)
  : QWebView(parent)
{
    setup(account, jar);
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

void ShibbolethWebView::slotLoadStarted()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}

void ShibbolethWebView::slotLoadFinished(bool success)
{
    QApplication::restoreOverrideCursor();

    if (!title().isNull()) {
        setWindowTitle(tr("%1 - %2").arg(Theme::instance()->appNameGUI(), title()));
    }

    if (!success) {
        QMessageBox::critical(this, tr("Error loading IdP login page"),
                              tr("Could not load Shibboleth login page to log you in.\n"
                                 "Please ensure that your network connection is working."));

    }
}

} // ns Mirall

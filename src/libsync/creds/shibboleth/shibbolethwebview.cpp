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
#include <QNetworkCookieJar>
#include <QWebFrame>
#include <QWebPage>
#include <QMessageBox>
#include <QNetworkReply>

#include "creds/shibboleth/shibbolethwebview.h"
#include "creds/shibbolethcredentials.h"
#include "account.h"
#include "logger.h"
#include "accessmanager.h"
#include "theme.h"

namespace OCC
{

ShibbolethWebView::ShibbolethWebView(Account* account, QWidget* parent)
    : QWebView(parent)
    , _account(account)
    , _accepted(false)
    , _cursorOverriden(false)
{
    // no minimize
    setWindowFlags(Qt::Dialog);
    setAttribute(Qt::WA_DeleteOnClose);
    QWebPage* page = new QWebPage(this);
    page->setNetworkAccessManager(account->networkAccessManager());
    connect(page, SIGNAL(loadStarted()),
            this, SLOT(slotLoadStarted()));
    connect(page, SIGNAL(loadFinished(bool)),
            this, SLOT(slotLoadFinished(bool)));


    connect(page->networkAccessManager()->cookieJar(),
            SIGNAL(newCookiesForUrl (QList<QNetworkCookie>, QUrl)),
            this, SLOT(onNewCookiesForUrl (QList<QNetworkCookie>, QUrl)));
    page->mainFrame()->load(account->url());
    this->setPage(page);
    setWindowTitle(tr("%1 - Authenticate").arg(Theme::instance()->appNameGUI()));

    // If we have a valid cookie, it's most likely expired. We can use this as
    // as a criteria to tell the user why the browser window pops up
    QNetworkCookie shibCookie = ShibbolethCredentials::findShibCookie(_account, ShibbolethCredentials::accountCookies(_account));
    if (shibCookie != QNetworkCookie()) {
        Logger::instance()->postOptionalGuiLog(tr("Reauthentication required"), tr("Your session has expired. You need to re-login to continue to use the client."));
    }
}

ShibbolethWebView::~ShibbolethWebView()
{
}

void ShibbolethWebView::onNewCookiesForUrl (const QList<QNetworkCookie>& cookieList, const QUrl& url)
{
    if (url.host() == _account->url().host()) {
        QNetworkCookie shibCookie = ShibbolethCredentials::findShibCookie(_account, cookieList);
        if (shibCookie != QNetworkCookie()) {
            Q_EMIT shibbolethCookieReceived(shibCookie, _account);
            accept();
            close();
        }
    }
}

void ShibbolethWebView::closeEvent(QCloseEvent *event)
{
    if (_cursorOverriden) {
        QApplication::restoreOverrideCursor();
    }

    if (!_accepted) {
        Q_EMIT rejected();
    }
    QWebView::closeEvent(event);
}

void ShibbolethWebView::slotLoadStarted()
{
    if (!_cursorOverriden) {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        _cursorOverriden = true;
    }
}

void ShibbolethWebView::slotLoadFinished(bool success)
{
    if (_cursorOverriden) {
        QApplication::restoreOverrideCursor();
    }

    if (!title().isNull()) {
        setWindowTitle(tr("%1 - %2").arg(Theme::instance()->appNameGUI(), title()));
    }

    if (!success) {
        qDebug() << Q_FUNC_INFO << "Could not load Shibboleth login page to log you in.";

    }
}

void ShibbolethWebView::accept()
{
    _accepted = true;
}

} // namespace OCC

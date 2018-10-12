/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_WIZARD_SHIBBOLETH_WEB_VIEW_H
#define MIRALL_WIZARD_SHIBBOLETH_WEB_VIEW_H

#include "owncloudlib.h"
#include <QList>
#include <QPointer>
#include <QWebView>
#include "accountfwd.h"

class QNetworkCookie;
class QUrl;

namespace OCC {

class ShibbolethCookieJar;

/**
 * @brief The ShibbolethWebView class
 * @ingroup gui
 */
class ShibbolethWebView : public QWebView
{
    Q_OBJECT

public:
    ShibbolethWebView(AccountPtr account, QWidget *parent = Q_NULLPTR);
    ShibbolethWebView(AccountPtr account, ShibbolethCookieJar *jar, QWidget *parent = Q_NULLPTR);
    ~ShibbolethWebView();

    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

Q_SIGNALS:
    void shibbolethCookieReceived(const QNetworkCookie &cookie);
    void rejected();

private Q_SLOTS:
    void onNewCookiesForUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url);
    void slotLoadStarted();
    void slotLoadFinished(bool success);

protected:
    void accept();

private:
    void setup(AccountPtr account, ShibbolethCookieJar *jar);
    AccountPtr _account;
    bool _accepted;
    bool _cursorOverriden;
};

} // namespace OCC

#endif

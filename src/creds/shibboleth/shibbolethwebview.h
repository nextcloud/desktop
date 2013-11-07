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

#ifndef MIRALL_WIZARD_SHIBBOLETH_WEB_VIEW_H
#define MIRALL_WIZARD_SHIBBOLETH_WEB_VIEW_H

#include <QList>
#include <QWebView>

class QNetworkCookie;
class QUrl;

namespace Mirall
{

class ShibbolethCookieJar;
class Account;

class ShibbolethWebView : public QWebView
{
  Q_OBJECT

public:
  ShibbolethWebView(Account *account, QWidget* parent = 0);
  ShibbolethWebView(Account *account, ShibbolethCookieJar* jar, QWidget* parent = 0);
  ~ShibbolethWebView();

protected:
  void hideEvent(QHideEvent* event);

Q_SIGNALS:
  void shibbolethCookieReceived(const QNetworkCookie& cookie);
  void viewHidden();
  void otherCookiesReceived(const QList<QNetworkCookie>& cookieList, const QUrl& url);

private Q_SLOTS:
  void onNewCookiesForUrl(const QList<QNetworkCookie>& cookieList, const QUrl& url);
  void slotLoadStarted();
  void slotLoadFinished(bool success = true);

private:
  void setup(Account *account, ShibbolethCookieJar* jar);
};

} // ns Mirall

#endif

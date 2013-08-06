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

#ifndef MIRALL_OWNCLOUD_SHIBBOLETH_CREDS_PAGE_H
#define MIRALL_OWNCLOUD_SHIBBOLETH_CREDS_PAGE_H

#include <QList>
#include <QMap>
#include <QNetworkCookie>
#include <QUrl>

#include "wizard/abstractcredswizardpage.h"

namespace Mirall {

class ShibbolethWebView;

class OwncloudShibbolethCredsPage : public AbstractCredentialsWizardPage
{
  Q_OBJECT
public:
  OwncloudShibbolethCredsPage();

  AbstractCredentials* getCredentials() const;

  void initializePage();
  int nextId() const;
  void setConnected();

Q_SIGNALS:
  void connectToOCUrl(const QString&);

public Q_SLOTS:
  void setVisible(bool visible);

private Q_SLOTS:
  void slotShibbolethCookieReceived(const QNetworkCookie& cookie);
  void slotOtherCookiesReceived(const QList<QNetworkCookie>& cookieList, const QUrl& url);
  void slotViewHidden();

private:
  void setupBrowser();
  void disposeBrowser();

  ShibbolethWebView* _browser;
  QNetworkCookie _cookie;
  bool _afterInitialSetup;
  QMap<QUrl, QList<QNetworkCookie> > _cookiesForUrl;
};

} // ns Mirall

#endif

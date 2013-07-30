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

#include <QNetworkCookie>

#include "mirall/wizard/abstractcredswizardpage.h"

#include "ui_owncloudshibbolethcredspage.h"

namespace Mirall {

class ShibbolethWebView;

class OwncloudShibbolethCredsPage : public AbstractCredentialsWizardPage
{
  Q_OBJECT
public:
  OwncloudShibbolethCredsPage();

  AbstractCredentials* getCredentials() const;

  bool isComplete() const;
  void initializePage();
  void cleanupPage();
  bool validatePage();
  int nextId() const;
  void setConnected(bool connected);
  void setErrorString(const QString& err);

Q_SIGNALS:
  void connectToOCUrl(const QString&);

private Q_SLOTS:
  void onShibbolethCookieReceived(const QNetworkCookie& cookie);

private:
  enum Stage {
    INITIAL_STEP,
    GOT_COOKIE,
    CHECKING,
    CONNECTED
  };

  void setupCustomization();
  void disposeBrowser(bool later);

  Ui_OwncloudShibbolethCredsPage _ui;
  Stage _stage;
  ShibbolethWebView* _browser;
  QNetworkCookie _cookie;
};

} // ns Mirall

#endif

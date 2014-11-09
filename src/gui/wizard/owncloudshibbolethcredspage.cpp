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

#include <QVariant>

#include "wizard/owncloudshibbolethcredspage.h"
#include "theme.h"
#include "account.h"
#include "cookiejar.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"
#include "creds/shibbolethcredentials.h"
#include "creds/shibboleth/shibbolethwebview.h"

namespace OCC
{

OwncloudShibbolethCredsPage::OwncloudShibbolethCredsPage()
    : AbstractCredentialsWizardPage(),
      _browser(0),
      _afterInitialSetup(false)
{}

void OwncloudShibbolethCredsPage::setupBrowser()
{
    if (!_browser.isNull()) {
        return;
    }
    OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard*>(wizard());
    Account *account = ocWizard->account();

    // we need to reset the cookie jar to drop temporary cookies (like the shib cookie)
    // i.e. if someone presses "back"
    QNetworkAccessManager *qnam = account->networkAccessManager();
    CookieJar *jar = new CookieJar;
    // Implicitly deletes the old cookie jar, and reparents the jar
    qnam->setCookieJar(jar);

    _browser = new ShibbolethWebView(account);
    connect(_browser, SIGNAL(shibbolethCookieReceived(const QNetworkCookie&, Account*)),
            this, SLOT(slotShibbolethCookieReceived(const QNetworkCookie&, Account*)), Qt::QueuedConnection);
    connect(_browser, SIGNAL(rejected()),
            this, SLOT(slotBrowserRejected()));

    _browser->move(ocWizard->x(), ocWizard->y());
    _browser->show();
    _browser->setFocus();
}

void OwncloudShibbolethCredsPage::setVisible(bool visible)
{
    if (!_afterInitialSetup) {
        QWizardPage::setVisible(visible);
        return;
    }

    if (isVisible() == visible) {
        return;
    }
    if (visible) {
        setupBrowser();
        wizard()->hide();
    } else {
        wizard()->show();
    }
}

void OwncloudShibbolethCredsPage::initializePage()
{
    _afterInitialSetup = true;
}

int OwncloudShibbolethCredsPage::nextId() const
{
  return WizardCommon::Page_AdvancedSetup;
}

void OwncloudShibbolethCredsPage::setConnected()
{
    wizard()->show();
}

AbstractCredentials* OwncloudShibbolethCredsPage::getCredentials() const
{
    const OwncloudWizard *ocWizard = static_cast<const OwncloudWizard*>(wizard());
    Account *account = ocWizard->account();

    return new ShibbolethCredentials(_cookie, account);
}

void OwncloudShibbolethCredsPage::slotShibbolethCookieReceived(const QNetworkCookie &cookie, Account*)
{
    _cookie = cookie;
    emit connectToOCUrl(field("OCUrl").toString().simplified());
}

void OwncloudShibbolethCredsPage::slotBrowserRejected()
{
    wizard()->back();
    wizard()->show();
}

} // namespace OCC

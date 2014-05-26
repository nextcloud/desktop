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
#include "mirall/theme.h"
#include "mirall/account.h"
#include "mirall/cookiejar.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"
#include "creds/shibbolethcredentials.h"
#include "creds/shibboleth/shibbolethwebview.h"

namespace Mirall
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
    delete qnam->cookieJar();
    CookieJar *jar = new CookieJar;
    qnam->setCookieJar(jar);
    jar->setParent(0);

    _browser = new ShibbolethWebView(account);
    connect(_browser, SIGNAL(shibbolethCookieReceived(const QNetworkCookie&, Account*)),
            this, SLOT(slotShibbolethCookieReceived()));
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
    return new ShibbolethCredentials;
}

void OwncloudShibbolethCredsPage::slotShibbolethCookieReceived()
{
    emit connectToOCUrl(field("OCUrl").toString().simplified());
}

void OwncloudShibbolethCredsPage::slotBrowserRejected()
{
    wizard()->back();
    wizard()->show();
}

} // ns Mirall

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
    AccountPtr account = ocWizard->account();

    // we need to reset the cookie jar to drop temporary cookies (like the shib cookie)
    // i.e. if someone presses "back"
    QNetworkAccessManager *qnam = account->networkAccessManager();
    CookieJar *jar = new CookieJar;
    jar->restore(account->cookieJarPath());
    // Implicitly deletes the old cookie jar, and reparents the jar
    qnam->setCookieJar(jar);

    _browser = new ShibbolethWebView(account);
    connect(_browser, SIGNAL(shibbolethCookieReceived(const QNetworkCookie&)),
            this, SLOT(slotShibbolethCookieReceived(const QNetworkCookie&)), Qt::QueuedConnection);
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
    return new ShibbolethCredentials(_cookie);
}

void OwncloudShibbolethCredsPage::slotShibbolethCookieReceived(const QNetworkCookie &cookie)
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

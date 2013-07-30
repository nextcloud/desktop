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

#include "mirall/wizard/owncloudshibbolethcredspage.h"
#include "mirall/theme.h"
#include "mirall/wizard/owncloudwizardcommon.h"
#include "mirall/creds/shibbolethcredentials.h"
#include "mirall/creds/shibboleth/shibbolethwebview.h"

namespace Mirall
{

OwncloudShibbolethCredsPage::OwncloudShibbolethCredsPage()
    : AbstractCredentialsWizardPage(),
      _ui(),
      _stage(INITIAL_STEP),
      _browser(0),
      _cookie()
{
    _ui.setupUi(this);

    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(Theme::instance()->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Process through Shibboleth form")));

    setupCustomization();
}

void OwncloudShibbolethCredsPage::setupCustomization()
{
    // set defaults for the customize labels.
    _ui.topLabel->hide();
    _ui.bottomLabel->hide();

    Theme *theme = Theme::instance();
    QVariant variant = theme->customMedia( Theme::oCSetupTop );
    if( !variant.isNull() ) {
        WizardCommon::setupCustomMedia( variant, _ui.topLabel );
    }

    variant = theme->customMedia( Theme::oCSetupBottom );
    WizardCommon::setupCustomMedia( variant, _ui.bottomLabel );
}

bool OwncloudShibbolethCredsPage::isComplete() const
{
    return _stage == GOT_COOKIE;
}

void OwncloudShibbolethCredsPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);
    _browser = new ShibbolethWebView(QUrl(field("OCUrl").toString().simplified()), this);

    connect(_browser, SIGNAL(shibbolethCookieReceived(QNetworkCookie)),
            this, SLOT(onShibbolethCookieReceived(QNetworkCookie)));

    _ui.contentLayout->insertWidget(0, _browser);
    _browser->show();
    _browser->setFocus();
    _ui.infoLabel->show();
    _ui.infoLabel->setText(tr("Please follow the steps on displayed page above"));
    _stage = INITIAL_STEP;
    _cookie = QNetworkCookie();
}

void OwncloudShibbolethCredsPage::disposeBrowser(bool later)
{
    if (_browser) {
        _browser->hide();
        disconnect(_browser, SIGNAL(shibbolethCookieReceived(QNetworkCookie)),
                   this, SLOT(onShibbolethCookieReceived(QNetworkCookie)));
        if (later) {
            _browser->deleteLater();
        } else {
            delete _browser;
        }
        _browser = 0;
    }
}

void OwncloudShibbolethCredsPage::cleanupPage()
{
    disposeBrowser(false);
}

bool OwncloudShibbolethCredsPage::validatePage()
{
    switch (_stage) {
    case INITIAL_STEP:
        return false;

    case GOT_COOKIE:
        _stage = CHECKING;
        emit completeChanged();
        emit connectToOCUrl(field("OCUrl").toString().simplified());
        return false;

    case CHECKING:
        return false;

    case CONNECTED:
        return true;
    }

    return false;
}

int OwncloudShibbolethCredsPage::nextId() const
{
  return WizardCommon::Page_Result;
}

void OwncloudShibbolethCredsPage::setConnected( bool comp )
{
    if (comp) {
        _stage = CONNECTED;
    } else {
        // sets stage to INITIAL
        initializePage();
    }
    emit completeChanged();
}

void OwncloudShibbolethCredsPage::setErrorString(const QString& err)
{
    if( err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        initializePage();
        _ui.errorLabel->setVisible(true);
        _ui.errorLabel->setText(err);
    }
    emit completeChanged();
}

AbstractCredentials* OwncloudShibbolethCredsPage::getCredentials() const
{
    return new ShibbolethCredentials(_cookie);
}

void OwncloudShibbolethCredsPage::onShibbolethCookieReceived(const QNetworkCookie& cookie)
{
    disposeBrowser(true);
    _stage = GOT_COOKIE;
    _cookie = cookie;
    _ui.infoLabel->setText("Please click \"Connect\" to check received Shibboleth session.");
    emit completeChanged();
}

} // ns Mirall

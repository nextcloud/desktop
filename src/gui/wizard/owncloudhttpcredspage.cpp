/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "QProgressIndicator.h"

#include "creds/httpcredentials.h"
#include "theme.h"
#include "account.h"
#include "configfile.h"
#include "wizard/owncloudhttpcredspage.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"

namespace OCC
{

OwncloudHttpCredsPage::OwncloudHttpCredsPage()
  : AbstractCredentialsWizardPage(),
    _ui(),
    _connected(false),
    _checking(false),
    _configExists(false),
    _progressIndi(new QProgressIndicator (this))
{
    _ui.setupUi(this);

    registerField( QLatin1String("OCUser*"),   _ui.leUsername);
    registerField( QLatin1String("OCPasswd*"), _ui.lePassword);

    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(Theme::instance()->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Enter user credentials")));

    _ui.resultLayout->addWidget( _progressIndi );
    stopSpinner();
    setupCustomization();
}

void OwncloudHttpCredsPage::setupCustomization()
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

void OwncloudHttpCredsPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    OwncloudWizard* ocWizard = qobject_cast< OwncloudWizard* >(wizard());
    AbstractCredentials *cred = ocWizard->account()->credentials();
    HttpCredentials *httpCreds = qobject_cast<HttpCredentials*>(cred);
    if (httpCreds) {
        const QString user = httpCreds->fetchUser(ocWizard->account());
        if (!user.isEmpty()) {
            _ui.leUsername->setText(user);
        }
    }
    _ui.leUsername->setFocus();
}

void OwncloudHttpCredsPage::cleanupPage()
{
    _ui.leUsername->clear();
    _ui.lePassword->clear();
}

bool OwncloudHttpCredsPage::validatePage()
{
    if (_ui.leUsername->text().isEmpty() || _ui.lePassword->text().isEmpty()) {
        return false;
    }

    if (!_connected) {
        _ui.errorLabel->setVisible(false);
        _checking = true;
        startSpinner();
        emit completeChanged();
        emit connectToOCUrl(field("OCUrl").toString().simplified());

        return false;
    } else {
        _checking = false;
        emit completeChanged();
        stopSpinner();
        return true;
    }
    return true;
}

int OwncloudHttpCredsPage::nextId() const
{
    return WizardCommon::Page_AdvancedSetup;
}

void OwncloudHttpCredsPage::setConnected( bool comp )
{
    _connected = comp;
    stopSpinner ();
}

void OwncloudHttpCredsPage::startSpinner()
{
    _ui.resultLayout->setEnabled(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();
}

void OwncloudHttpCredsPage::stopSpinner()
{
    _ui.resultLayout->setEnabled(false);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

void OwncloudHttpCredsPage::setErrorString(const QString& err)
{
    if( err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        _ui.errorLabel->setVisible(true);
        _ui.errorLabel->setText(err);
    }
    _checking = false;
    emit completeChanged();
    stopSpinner();
}

AbstractCredentials* OwncloudHttpCredsPage::getCredentials() const
{
    return new HttpCredentialsGui(_ui.leUsername->text(), _ui.lePassword->text());
}

void OwncloudHttpCredsPage::setConfigExists(bool config)
{
    _configExists = config;

    if (config == true) {
        setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Update user credentials")));
    }
}

} // namespace OCC

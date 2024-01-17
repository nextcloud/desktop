/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#include "oauthcredentialssetupwizardpage.h"
#include "ui_credentialssetupwizardpage.h"

#include "gui/guiutility.h"
#include "theme.h"

#include <QClipboard>

namespace OCC::Wizard {

OAuthCredentialsSetupWizardPage::OAuthCredentialsSetupWizardPage(const QUrl &serverUrl)
    : _ui(new ::Ui::CredentialsSetupWizardPage)
{
    _ui->setupUi(this);

    // bring the correct widget to the front
    _ui->contentWidget->setCurrentWidget(_ui->oauthLoginWidget);

    _ui->urlLabel->setText(tr("Connecting to <a href='%1' style='color: %2;'>%1</a>").arg(serverUrl.toString(), Theme::instance()->wizardHeaderTitleColor().name()));

    // we want to give the user a chance to preserve their privacy when using a private proxy for instance
    // therefore, we need to make sure the user can manually
    // using clicked allows a user to "abort the click" (unlike pressed and released)
    connect(_ui->oauthLoginWidget, &OAuthLoginWidget::openBrowserButtonClicked, this, [this](const QUrl &url) {
        Q_EMIT openBrowserButtonPushed(url);

        // change button title after first click
        // TODO: move to OAuthLoginWidget
        _ui->oauthLoginWidget->setOpenBrowserButtonText(tr("Reopen Browser"));
    });
    connect(this, &AbstractSetupWizardPage::pageDisplayed, _ui->oauthLoginWidget, qOverload<>(&OAuthLoginWidget::setFocus));

    _ui->topLabel->setText(tr("Please use your browser to log in to %1.").arg(Theme::instance()->appNameGUI()));
}

OAuthCredentialsSetupWizardPage::~OAuthCredentialsSetupWizardPage()
{
    delete _ui;
}

bool OAuthCredentialsSetupWizardPage::validateInput()
{
    // in this special case, the input may never be validated, i.e., the next button also never needs to be enabled
    // an external system set up by the controller will move to the next page in the background
    return false;
}

void OAuthCredentialsSetupWizardPage::setAuthUrl(const QUrl &url)
{
    _ui->oauthLoginWidget->setUrl(url);
}

} // namespace OCC::Wizard

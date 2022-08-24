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

#include "basiccredentialssetupwizardpage.h"
#include "ui_credentialssetupwizardpage.h"

#include "theme.h"

namespace OCC::Wizard {

BasicCredentialsSetupWizardPage::BasicCredentialsSetupWizardPage(const QUrl &serverUrl)
    : _ui(new ::Ui::CredentialsSetupWizardPage)
{
    _ui->setupUi(this);

    _ui->topLabel->setText(tr("Please enter your credentials to log in to your account."));

    // bring the correct widget to the front
    _ui->contentWidget->setCurrentWidget(_ui->basicLoginWidget);

    const QString linkColor = Theme::instance()->wizardHeaderTitleColor().name();

    _ui->urlLabel->setText(tr("Connecting to <a href='%1' style='color: %2;'>%1</a>").arg(serverUrl.toString(), linkColor));

    connect(this, &AbstractSetupWizardPage::pageDisplayed, this, [this]() {
        _ui->basicLoginWidget->setFocus();
    });
}

BasicCredentialsSetupWizardPage *BasicCredentialsSetupWizardPage::createForWebFinger(const QUrl &serverUrl, const QString &username)
{
    auto page = new BasicCredentialsSetupWizardPage(serverUrl);
    page->_ui->basicLoginWidget->forceUsername(username);
    return page;
}

QString BasicCredentialsSetupWizardPage::username() const
{
    return _ui->basicLoginWidget->username();
}

QString BasicCredentialsSetupWizardPage::password() const
{
    return _ui->basicLoginWidget->password();
}

BasicCredentialsSetupWizardPage::~BasicCredentialsSetupWizardPage()
{
    delete _ui;
}

bool BasicCredentialsSetupWizardPage::validateInput()
{
    return !(username().isEmpty() || password().isEmpty());
}
}

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
#include "ui_basiccredentialssetupwizardpage.h"

#include "theme.h"

namespace OCC::Wizard {

BasicCredentialsSetupWizardPage::BasicCredentialsSetupWizardPage(const QUrl &serverUrl)
    : _ui(new ::Ui::BasicCredentialsSetupWizardPage)
{
    _ui->setupUi(this);

    _ui->urlLabel->setText(tr("Connecting to <a href='%1' style='color: %2;'>%1</a>").arg(serverUrl.toString(), Theme::instance()->wizardHeaderTitleColor().name()));

    connect(this, &AbstractSetupWizardPage::pageDisplayed, this, [this]() {
        _ui->usernameLineEdit->setFocus();
    });
}

QString BasicCredentialsSetupWizardPage::username() const
{
    return _ui->usernameLineEdit->text();
}

QString BasicCredentialsSetupWizardPage::password() const
{
    return _ui->passwordLineEdit->text();
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

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

#include "webfingersetupwizardpage.h"
#include "ui_webfingersetupwizardpage.h"

#include "theme.h"

namespace OCC::Wizard {

WebFingerSetupWizardPage::WebFingerSetupWizardPage(const QUrl &serverUrl)
    : _ui(new ::Ui::WebFingerSetupWizardPage)
{
    _ui->setupUi(this);

    _ui->urlLabel->setText(tr("Connecting to <a href='%1' style='color: %2;'>%1</a>").arg(serverUrl.toString(), Theme::instance()->wizardHeaderTitleColor().name()));

    connect(this, &AbstractSetupWizardPage::pageDisplayed, this, [this]() {
        _ui->usernameLineEdit->setFocus();
    });

    const QString usernameLabelText = []() {
        const auto userIdType = Theme::instance()->userIDType();

        switch (userIdType) {
        case Theme::UserIDCustom:
            return Theme::instance()->customUserID();
        default:
            return Utility::enumToDisplayName(Theme::instance()->userIDType());
        }
    }();

    _ui->usernameLabel->setText(usernameLabelText);
    _ui->usernameLineEdit->setPlaceholderText(usernameLabelText);

    if (!Theme::instance()->userIDHint().isEmpty()) {
        _ui->usernameLineEdit->setPlaceholderText(Theme::instance()->userIDHint());
    }

    // in contrast to the basic login widget, we need to adjust the label above the form here, too
    _ui->enterYourUsernameLabel->setText(tr("Please enter your %1:", "Please enter your [username, e-mail address, ...]:").arg(usernameLabelText));

    connect(_ui->usernameLineEdit, &QLineEdit::textChanged, this, &AbstractSetupWizardPage::contentChanged);
}

QString WebFingerSetupWizardPage::username() const
{
    return _ui->usernameLineEdit->text();
}

WebFingerSetupWizardPage::~WebFingerSetupWizardPage()
{
    delete _ui;
}

bool WebFingerSetupWizardPage::validateInput()
{
    return !(username().isEmpty());
}
}

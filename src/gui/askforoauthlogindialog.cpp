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

#include "askforoauthlogindialog.h"
#include "application.h"
#include "creds/httpcredentialsgui.h"
#include "theme.h"
#include "ui_askforoauthlogindialog.h"

#include <QClipboard>

namespace OCC {

AskForOAuthLoginDialog::AskForOAuthLoginDialog(AccountPtr accountPtr, QWidget *parent)
    : QDialog(parent)
    , _ui(new ::Ui::AskForOAuthLoginDialog)
{
    _ui->setupUi(this);

    _ui->label->setText(tr("The account %1 is currently logged out.\n\nPlease authenticate using your browser.").arg(accountPtr->displayName()));
    _ui->iconLabel->setPixmap(Theme::instance()->applicationIcon().pixmap(64, 64));

    setModal(true);

    setFixedSize(this->sizeHint());

    auto creds = qobject_cast<HttpCredentialsGui *>(accountPtr->credentials());
    Q_ASSERT(creds != nullptr);

    connect(creds, &HttpCredentialsGui::authorisationLinkChanged, this, [this]() {
        _ui->copyUrlToClipboardButton->setEnabled(true);
        _ui->openBrowserButton->setEnabled(true);
    });

    connect(_ui->openBrowserButton, &QPushButton::clicked, this, [this, accountPtr]() {
        qobject_cast<HttpCredentialsGui *>(accountPtr->credentials())->openBrowser();
        _ui->openBrowserButton->setText(tr("Reopen browser"));
    });
    connect(_ui->copyUrlToClipboardButton, &QPushButton::clicked, this, [accountPtr]() {
        // TODO: use authorisationLinkAsync
        auto link = qobject_cast<HttpCredentialsGui *>(accountPtr->credentials())->authorisationLink().toString();
        ocApp()->clipboard()->setText(link);
    });
}

AskForOAuthLoginDialog::~AskForOAuthLoginDialog()
{
    delete _ui;
}

} // OCC

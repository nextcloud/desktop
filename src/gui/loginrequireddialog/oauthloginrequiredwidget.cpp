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

#include "oauthloginrequiredwidget.h"
#include "ui_oauthloginrequiredwidget.h"

#include "gui/application.h"
#include "gui/creds/httpcredentialsgui.h"
#include "gui/guiutility.h"

#include <QClipboard>

namespace OCC {

OAuthLoginRequiredWidget::OAuthLoginRequiredWidget(AccountPtr accountPtr, QWidget *parent)
    : AbstractLoginRequiredWidget(parent)
    , _ui(new ::Ui::OAuthLoginRequiredWidget)
    , _openBrowserButton(new QPushButton(tr("Open Browser"), this))
    , _copyURLToClipboardButton(new QPushButton(Utility::getCoreIcon(QStringLiteral("clipboard")), tr("Copy URL to clipboard"), this))
{
    _ui->setupUi(this);

    Utility::setModal(this);

    _ui->loginRequiredLabel->setText(tr("The account %1 is currently logged out.\n\nPlease authenticate using your browser.").arg(accountPtr->displayName()));

    auto creds = qobject_cast<HttpCredentialsGui *>(accountPtr->credentials());
    Q_ASSERT(creds != nullptr);

    connect(creds, &HttpCredentialsGui::authorisationLinkChanged, this, [this]() {
        _copyURLToClipboardButton->setEnabled(true);
        _openBrowserButton->setEnabled(true);
    });

    connect(_openBrowserButton, &QPushButton::clicked, this, [this, accountPtr]() {
        qobject_cast<HttpCredentialsGui *>(accountPtr->credentials())->openBrowser();
        _openBrowserButton->setText(tr("Reopen browser"));
    });
    connect(_copyURLToClipboardButton, &QPushButton::clicked, this, [accountPtr]() {
        // TODO: use authorisationLinkAsync
        auto link = qobject_cast<HttpCredentialsGui *>(accountPtr->credentials())->authorisationLink().toString();
        ocApp()->clipboard()->setText(link);
    });
}

OAuthLoginRequiredWidget::~OAuthLoginRequiredWidget()
{
    delete _ui;
}

QList<QPair<QAbstractButton *, QDialogButtonBox::ButtonRole>> OAuthLoginRequiredWidget::buttons()
{
    return {
        { _openBrowserButton, QDialogButtonBox::ActionRole },
        { _copyURLToClipboardButton, QDialogButtonBox::ActionRole },
    };
}

}

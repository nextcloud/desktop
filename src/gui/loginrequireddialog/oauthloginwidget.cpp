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

#include "oauthloginwidget.h"
#include "ui_oauthloginwidget.h"

#include "gui/application.h"
#include "gui/creds/httpcredentialsgui.h"
#include "gui/guiutility.h"

#include <QClipboard>

namespace OCC {

OAuthLoginWidget::OAuthLoginWidget(QWidget *parent)
    : AbstractLoginWidget(parent)
    , _ui(new ::Ui::OAuthLoginWidget)
{
    _ui->setupUi(this);

    Utility::setModal(this);

    connect(_ui->openBrowserButton, &QPushButton::clicked, this, &OAuthLoginWidget::openBrowserButtonClicked);
    connect(_ui->copyUrlToClipboardButton, &QPushButton::clicked, this, &OAuthLoginWidget::copyUrlToClipboardButtonClicked);

    // depending on the theme we have to use a light or dark icon
    _ui->copyUrlToClipboardButton->setIcon(Utility::getCoreIcon(QStringLiteral("copy")));

    setFocusProxy(_ui->openBrowserButton);
}

OAuthLoginWidget::~OAuthLoginWidget()
{
    delete _ui;
}

void OAuthLoginWidget::setOpenBrowserButtonText(const QString &newText)
{
    _ui->openBrowserButton->setText(newText);
}

}

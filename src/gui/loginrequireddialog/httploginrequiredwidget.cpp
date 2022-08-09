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

#include "httploginrequiredwidget.h"
#include "ui_httploginrequiredwidget.h"

#include "creds/httpcredentials.h"
#include "gui/guiutility.h"

#include <QPushButton>
#include <QString>

namespace OCC {

HttpLoginRequiredWidget::HttpLoginRequiredWidget(AccountPtr accountPtr, QWidget *parent)
    : AbstractLoginRequiredWidget(parent)
    , _ui(new ::Ui::HttpLoginRequiredWidget)
    , _logInButton(new QPushButton(tr("Log in"), this))
{
    _ui->setupUi(this);

    Utility::setModal(this);

    const QString appPasswordUrl = QStringLiteral("%1/settings/personal?sectionid=security#apppasswords").arg(accountPtr->url().toString());

    _ui->loginRequiredLabel->setText(tr("The account %1 is currently logged out.").arg(accountPtr->displayName()));
    _ui->appPasswordLabel->setText(tr("Click <a href='%1'>here</a> to request an app password on the server.").arg(appPasswordUrl));

    _ui->usernameLineEdit->setText(accountPtr->davUser());
}

HttpLoginRequiredWidget::~HttpLoginRequiredWidget()
{
    delete _ui;
}
QList<QPair<QAbstractButton *, QDialogButtonBox::ButtonRole>> HttpLoginRequiredWidget::buttons()
{
    return {
        { _logInButton, QDialogButtonBox::AcceptRole },
    };
}

QString HttpLoginRequiredWidget::password() const
{
    return _ui->passwordLineEdit->text();
}

} // OCC

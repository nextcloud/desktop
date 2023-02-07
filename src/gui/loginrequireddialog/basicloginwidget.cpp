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

#include "basicloginwidget.h"
#include "ui_basicloginwidget.h"

#include "creds/httpcredentials.h"
#include "gui/guiutility.h"
#include "theme.h"

#include <QPushButton>
#include <QString>

namespace OCC {

BasicLoginWidget::BasicLoginWidget(QWidget *parent)
    : AbstractLoginWidget(parent)
    , _ui(new ::Ui::BasicLoginWidget)
{
    _ui->setupUi(this);

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

    qDebug() << _ui->usernameLabel->isEnabled();
    qDebug() << _ui->usernameLineEdit->isEnabled();

    Utility::setModal(this);

    setFocusProxy(_ui->usernameLineEdit);

    connect(_ui->usernameLineEdit, &QLineEdit::textChanged, this, &AbstractLoginWidget::contentChanged);
    connect(_ui->passwordLineEdit, &QLineEdit::textChanged, this, &AbstractLoginWidget::contentChanged);
}

BasicLoginWidget::~BasicLoginWidget()
{
    delete _ui;
}

void BasicLoginWidget::forceUsername(const QString &username)
{
    _ui->usernameLineEdit->setText(username);
    _ui->usernameLineEdit->setEnabled(false);

    setFocusProxy(_ui->passwordLineEdit);
}

QString BasicLoginWidget::username()
{
    return _ui->usernameLineEdit->text();
}

QString BasicLoginWidget::password()
{
    return _ui->passwordLineEdit->text();
}

} // OCC

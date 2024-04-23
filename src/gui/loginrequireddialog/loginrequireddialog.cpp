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

#include "loginrequireddialog.h"
#include "ui_loginrequireddialog.h"

#include "theme.h"

#include <QClipboard>

namespace OCC {

LoginRequiredDialog::LoginRequiredDialog(Mode mode, QWidget *parent)
    : QWidget(parent)
    , _ui(new ::Ui::LoginRequiredDialog)
{
    _ui->setupUi(this);

    _ui->iconLabel->setPixmap(Theme::instance()->applicationIcon().pixmap(128, 128));

    // using a stacked widget appears to work better than a plain widget
    // we do this in the setup wizard as well
    _ui->contentWidget->setCurrentWidget([this, mode]() -> QWidget * {
        switch (mode) {
        case Mode::Basic:
            return _ui->basicLoginWidget;
        case Mode::OAuth:
            return _ui->oauthLoginWidget;
        default:
            Q_UNREACHABLE();
        }
    }());
}

LoginRequiredDialog::~LoginRequiredDialog()
{
    delete _ui;
}

void LoginRequiredDialog::setTopLabelText(const QString &newText)
{
    _ui->topLabel->setText(newText);
}

QWidget *LoginRequiredDialog::contentWidget() const
{
    return _ui->contentWidget->currentWidget();
}

} // OCC

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

#include "gui/application.h"
#include "gui/creds/httpcredentialsgui.h"
#include "gui/guiutility.h"
#include "theme.h"

#include <QClipboard>

namespace OCC {

LoginRequiredDialog::LoginRequiredDialog(Mode mode, QWidget *parent)
    : QDialog(parent)
    , _ui(new ::Ui::LoginRequiredDialog)
{
    _ui->setupUi(this);

    _ui->iconLabel->setPixmap(Theme::instance()->applicationIcon().pixmap(128, 128));

    // we want a custom text, but we make use of the button box's built-in reject role
    _ui->rightButtonBox->addButton(tr("Log out"), QDialogButtonBox::RejectRole);

    // make plain reject/accept buttons work w/o additional effort on the caller side
    connect(_ui->rightButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(_ui->rightButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

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

    Utility::setModal(this);
    setFixedSize(this->minimumSize());
}

LoginRequiredDialog::~LoginRequiredDialog()
{
    delete _ui;
}

void LoginRequiredDialog::setTopLabelText(const QString &newText)
{
    _ui->topLabel->setText(newText);
}

void LoginRequiredDialog::addLogInButton()
{
    _ui->rightButtonBox->addButton(tr("Log in"), QDialogButtonBox::AcceptRole);
}

QWidget *LoginRequiredDialog::contentWidget() const
{
    return _ui->contentWidget->currentWidget();
}

} // OCC

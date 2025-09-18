/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "passwordinputdialog.h"
#include "ui_passwordinputdialog.h"

namespace OCC {

PasswordInputDialog::PasswordInputDialog(const QString &description, const QString &error, QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::PasswordInputDialog)
{
    _ui->setupUi(this);

    _ui->passwordLineEditLabel->setText(description);
    _ui->passwordLineEditLabel->setVisible(!description.isEmpty());

    _ui->labelErrorMessage->setText(error);
    _ui->labelErrorMessage->setVisible(!error.isEmpty());

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

PasswordInputDialog::~PasswordInputDialog() = default;

QString PasswordInputDialog::password() const
{
    return _ui->passwordLineEdit->text();
}
}

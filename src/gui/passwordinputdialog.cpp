/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "passwordinputdialog.h"
#include "ui_passwordinputdialog.h"

namespace OCC {

PasswordInputDialog::PasswordInputDialog(const QString &description, const QString &error, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PasswordInputDialog)
{
    ui->setupUi(this);

    ui->passwordLineEditLabel->setText(description);
    ui->passwordLineEditLabel->setVisible(!description.isEmpty());

    ui->labelErrorMessage->setText(error);
    ui->labelErrorMessage->setVisible(!error.isEmpty());

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

PasswordInputDialog::~PasswordInputDialog()
{
    delete ui;
}
QString PasswordInputDialog::password() const
{
    return ui->passwordLineEdit->text();
}
}

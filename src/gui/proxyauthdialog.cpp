/*
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "proxyauthdialog.h"
#include "ui_proxyauthdialog.h"

namespace OCC {

ProxyAuthDialog::ProxyAuthDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ProxyAuthDialog)
{
    ui->setupUi(this);
}

ProxyAuthDialog::~ProxyAuthDialog()
{
    delete ui;
}

void ProxyAuthDialog::setProxyAddress(const QString &address)
{
    ui->proxyAddress->setText(address);
}

QString ProxyAuthDialog::username() const
{
    return ui->usernameEdit->text();
}

QString ProxyAuthDialog::password() const
{
    return ui->passwordEdit->text();
}

void ProxyAuthDialog::reset()
{
    ui->usernameEdit->setFocus();
    ui->usernameEdit->clear();
    ui->passwordEdit->clear();
}

} // namespace OCC

/*
 * Copyright (C) 2014 by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "authenticationdialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace OCC {

AuthenticationDialog::AuthenticationDialog(const QString &realm, const QString &domain, QWidget *parent)
    : QDialog(parent)
    , _user(new QLineEdit)
    , _password(new QLineEdit)
{
    setWindowTitle(tr("Authentication Required"));
    QVBoxLayout *lay = new QVBoxLayout(this);
    QLabel *label = new QLabel(tr("Enter username and password for '%1' at %2.").arg(realm, domain));
    lay->addWidget(label);

    QFormLayout *form = new QFormLayout;
    form->addRow(tr("&User:"), _user);
    form->addRow(tr("&Password:"), _password);
    lay->addLayout(form);
    _password->setEchoMode(QLineEdit::Password);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal);
    connect(box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));
    lay->addWidget(box);
}

QString AuthenticationDialog::user() const
{
    return _user->text();
}

QString AuthenticationDialog::password() const
{
    return _password->text();
}

} // namespace OCC

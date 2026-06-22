/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    auto *lay = new QVBoxLayout(this);
    auto *label = new QLabel(tr("Enter username and password for \"%1\" at %2.").arg(realm, domain));
    label->setTextFormat(Qt::PlainText);
    lay->addWidget(label);

    auto *form = new QFormLayout;
    form->addRow(tr("&Username:"), _user);
    form->addRow(tr("&Password:"), _password);
    lay->addLayout(form);
    _password->setEchoMode(QLineEdit::Password);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
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

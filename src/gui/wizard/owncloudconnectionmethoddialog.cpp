/*
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wizard/owncloudconnectionmethoddialog.h"
#include <QUrl>

namespace OCC {

OwncloudConnectionMethodDialog::OwncloudConnectionMethodDialog(QWidget *parent)
    : QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::MSWindowsFixedSizeDialogHint)
    , ui(new Ui::OwncloudConnectionMethodDialog)
{
    ui->setupUi(this);

    connect(ui->btnNoTLS, &QAbstractButton::clicked, this, &OwncloudConnectionMethodDialog::returnNoTLS);
    connect(ui->btnClientSideTLS, &QAbstractButton::clicked, this, &OwncloudConnectionMethodDialog::returnClientSideTLS);
    connect(ui->btnBack, &QAbstractButton::clicked, this, &OwncloudConnectionMethodDialog::returnBack);
}

void OwncloudConnectionMethodDialog::setUrl(const QUrl &url)
{
    ui->label->setText(tr("<html><head/><body><p>Failed to connect to the secure server address <em>%1</em>. How do you wish to proceed?</p></body></html>").arg(url.toDisplayString().toHtmlEscaped()));
}

void OwncloudConnectionMethodDialog::setHTTPOnly(const bool retryHTTPonly)
{
    ui->btnNoTLS->setEnabled(retryHTTPonly);
}

void OwncloudConnectionMethodDialog::returnNoTLS()
{
    done(No_TLS);
}

void OwncloudConnectionMethodDialog::returnClientSideTLS()
{
    done(Client_Side_TLS);
}

void OwncloudConnectionMethodDialog::returnBack()
{
    done(Back);
}

OwncloudConnectionMethodDialog::~OwncloudConnectionMethodDialog()
{
    delete ui;
}
}

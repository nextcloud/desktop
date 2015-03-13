/*
 * Copyright (C) 2015 by nocteau
 * Copyright (C) 2015 by Daniel Molkentin <danimo@owncloud.com>
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

#include "ui_addcertificatedialog.h"
#include "addcertificatedialog.h"
#include <QFileDialog>
#include <QLineEdit>


namespace OCC {
AddCertificateDialog::AddCertificateDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddCertificateDialog)
{
    ui->setupUi(this);
    ui->labelErrorCertif->setText("");
}

AddCertificateDialog::~AddCertificateDialog()
{
    delete ui;
}

void AddCertificateDialog::on_pushButtonBrowseCertificate_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select a certificate"), "", tr("Certificate files (*.p12 *.pfx)"));
    ui->lineEditCertificatePath->setText(fileName);
}

QString AddCertificateDialog::getCertificatePath()
{
    return ui->lineEditCertificatePath->text();
}

QString AddCertificateDialog::getCertificatePasswd()
{
    return ui->lineEditPWDCertificate->text();
}

void AddCertificateDialog::showErrorMessage(const QString message)
{
    ui->labelErrorCertif->setText(message);
}

void AddCertificateDialog::reinit()
{
    ui->labelErrorCertif->clear();
    ui->lineEditCertificatePath->clear();
    ui->lineEditPWDCertificate->clear();
}

}

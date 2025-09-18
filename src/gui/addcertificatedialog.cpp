/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ui_addcertificatedialog.h"
#include "addcertificatedialog.h"
#include <QFileDialog>
#include <QLineEdit>


namespace OCC {
AddCertificateDialog::AddCertificateDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddCertificateDialog)
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

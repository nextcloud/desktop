/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ui_addcertificatedialog.h"
#include "addcertificatedialog.h"
#include <QFileDialog>
#include <QLineEdit>

#ifdef Q_OS_MACOS
#include "common/utility_mac_sandbox.h"
#endif


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
    // Use getSaveFileUrl for sandbox compatibility
    const auto fileUrl = QFileDialog::getOpenFileUrl(this, tr("Select a certificate"), QUrl(), tr("Certificate files (*.p12 *.pfx)"));
    
    if (fileUrl.isEmpty()) {
        return;
    }

#ifdef Q_OS_MACOS
    // On macOS with app sandbox, we need to verify we can access the security-scoped resource
    auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(fileUrl);
    
    if (!scopedAccess->isValid()) {
        ui->labelErrorCertif->setText(tr("Could not access the selected certificate file."));
        return;
    }
#endif

    ui->lineEditCertificatePath->setText(fileUrl.toLocalFile());
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

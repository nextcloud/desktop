/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ADDCERTIFICATEDIALOG_H
#define ADDCERTIFICATEDIALOG_H

#include <QDialog>
#include <QString>

namespace OCC {

namespace Ui {
    class AddCertificateDialog;
}

/**
 * @brief The AddCertificateDialog class
 * @ingroup gui
 */
class AddCertificateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddCertificateDialog(QWidget *parent = nullptr);
    ~AddCertificateDialog() override;
    QString getCertificatePath();
    QString getCertificatePasswd();
    void showErrorMessage(const QString message);
    void reinit();

private slots:
    void on_pushButtonBrowseCertificate_clicked();

private:
    Ui::AddCertificateDialog *ui;
};

} //End namespace OCC

#endif // ADDCERTIFICATEDIALOG_H

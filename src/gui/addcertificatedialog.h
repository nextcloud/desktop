/*
 * Copyright (C) 2015 by nocteau
 * Copyright (C) 2015 by Daniel Molkentin <danimo@owncloud.com>
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
    explicit AddCertificateDialog(QWidget *parent = 0);
    ~AddCertificateDialog();
    QString getCertificatePath();
    QString getCertificatePasswd();
    void showErrorMessage(const QString message);
    void reinit();

private slots:
    void on_pushButtonBrowseCertificate_clicked();

private:
    Ui::AddCertificateDialog *ui;

};

}//End namespace OCC

#endif // ADDCERTIFICATEDIALOG_H

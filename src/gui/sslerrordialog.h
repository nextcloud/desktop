/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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
#ifndef SSLERRORDIALOG_H
#define SSLERRORDIALOG_H

#include <QtCore>
#include <QDialog>
#include <QSslCertificate>
#include <QList>

#include "account.h"

class QSslError;
class QSslCertificate;

namespace OCC
{

namespace Ui {
class SslErrorDialog;
}

class SslDialogErrorHandler : public AbstractSslErrorHandler {
public:
    bool handleErrors(QList<QSslError> errors, QList<QSslCertificate> *certs, Account*) Q_DECL_OVERRIDE;
};

class SslErrorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SslErrorDialog(Account *account, QWidget *parent = 0);
    ~SslErrorDialog();
    bool checkFailingCertsKnown( const QList<QSslError> &errors );
    bool trustConnection();
    QList<QSslCertificate> unknownCerts() const { return _unknownCerts; }

private:
    QString styleSheet() const;
    bool _allTrusted;

    QString certDiv( QSslCertificate ) const;

    QList<QSslCertificate> _unknownCerts;
    QString                _customConfigHandle;
    Ui::SslErrorDialog     *_ui;
    Account                *_account;
};
} // end namespace

#endif // SSLERRORDIALOG_H

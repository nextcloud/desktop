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

namespace OCC {

namespace Ui {
    class SslErrorDialog;
}

/**
 * @brief The SslDialogErrorHandler class
 * @ingroup gui
 */
class SslDialogErrorHandler : public AbstractSslErrorHandler
{
public:
    bool handleErrors(QList<QSslError> errors, const QSslConfiguration &conf, QList<QSslCertificate> *certs, AccountPtr) override;
    bool checkFailingCertsKnown(const QList<QSslError> &errors, AccountPtr account);
    [[nodiscard]] QList<QSslCertificate> unknownCerts() const { return _unknownCerts; }

private:
    QList<QSslCertificate> _unknownCerts;
    QStringList _errorStrings;
    QStringList _additionalErrorStrings;
};

/**
 * @brief The SslErrorDialog class
 * @ingroup gui
 */
class SslErrorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SslErrorDialog(AccountPtr account, QWidget *parent = nullptr);
    ~SslErrorDialog() override;
    bool showFailingCertsKnown(
        const QList<QSslError> &errors,
        QList<QSslCertificate> unknownCerts,
        QStringList errorStrings,
        QStringList additionalErrorStrings
    );    bool trustConnection();
    [[nodiscard]] QList<QSslCertificate> unknownCerts() const { return _unknownCerts; }

private:
    [[nodiscard]] QString styleSheet() const;
    bool _allTrusted = false;

    [[nodiscard]] QString certDiv(QSslCertificate) const;

    QList<QSslCertificate> _unknownCerts;
    QString _customConfigHandle;
    Ui::SslErrorDialog *_ui;
    AccountPtr _account;
};
} // end namespace

#endif // SSLERRORDIALOG_H

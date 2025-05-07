/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    bool checkFailingCertsKnown(const QList<QSslError> &errors);
    bool trustConnection();
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

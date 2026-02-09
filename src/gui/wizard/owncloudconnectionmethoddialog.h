/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OWNCLOUDCONNECTIONMETHODDIALOG_H
#define OWNCLOUDCONNECTIONMETHODDIALOG_H

#include <QDialog>

#include "ui_owncloudconnectionmethoddialog.h"

namespace OCC {

namespace Ui {
    class OwncloudConnectionMethodDialog;
}

/**
 * @brief The OwncloudConnectionMethodDialog class
 * @ingroup gui
 */
class OwncloudConnectionMethodDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OwncloudConnectionMethodDialog(QWidget *parent = nullptr);
    ~OwncloudConnectionMethodDialog() override;
    enum {
        Closed = 0,
        No_TLS,
        Client_Side_TLS,
        Back
    };

    // The URL that was tried
    void setUrl(const QUrl &);
    void setHTTPOnly(bool);

public slots:
    void returnNoTLS();
    void returnClientSideTLS();
    void returnBack();

private:
    Ui::OwncloudConnectionMethodDialog *ui;
};
}

#endif // OWNCLOUDCONNECTIONMETHODDIALOG_H

/*
 * Copyright (C) 2015 by Jeroen Hoek
 * Copyright (C) 2015 by Olivier Goffart <ogoffart@owncloud.com>
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
    ~OwncloudConnectionMethodDialog();
    enum {
        Closed = 0,
        No_TLS,
        Client_Side_TLS,
        Back
    };

    // The URL that was tried
    void setUrl(const QUrl &);

public slots:
    void returnNoTLS();
    void returnClientSideTLS();
    void returnBack();

private:
    Ui::OwncloudConnectionMethodDialog *ui;
};
}

#endif // OWNCLOUDCONNECTIONMETHODDIALOG_H

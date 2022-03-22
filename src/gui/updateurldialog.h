/*
* Copyright (C) by Fabian Müller <fmueller@owncloud.com>
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

#pragma once

#include "account.h"

#include <QMessageBox>
#include <QUrl>
#include <QWidget>

namespace OCC {

/**
 * Compares two given URLs.
 * In case they differ, it asks the user whether they want to accept the change.
 * Otherwise, no dialog will be shown, and accept() will be emitted.
 * This special dialog cleans itself up one hidden.
 */
class UpdateUrlDialog : public QMessageBox
{
    Q_OBJECT
public:
    static UpdateUrlDialog *fromAccount(AccountPtr account, const QUrl &newUrl, QWidget *parent = nullptr);

    explicit UpdateUrlDialog(const QString &title, const QString &content, const QUrl &oldUrl, const QUrl &newUrl, QWidget *parent = nullptr);

private:
    QUrl _oldUrl;
    QUrl _newUrl;
};

}

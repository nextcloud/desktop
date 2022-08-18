/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#include "abstractloginwidget.h"
#include "account.h"
#include <QDialog>

namespace Ui {
class LoginRequiredDialog;
}

namespace OCC {

/**
 * This dialog is used to ask users to re-authenticate in case an existing account's credentials no longer work or the user logged out.
 * It is one of two locations in the code where we have users log in to their accounts (the other one is the setup wizard).
 */
class LoginRequiredDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginRequiredDialog(AbstractLoginWidget *contentWidget, QWidget *parent = nullptr);
    ~LoginRequiredDialog() override;

    void setTopLabelText(const QString &newText);

    /**
     * Add a "log in" button to the dialog. When clicked, the dialog is accepted.
     * For use with HTTP basic authentication.
     */
    void addLogInButton();

private:
    ::Ui::LoginRequiredDialog *_ui;
};

} // OCC

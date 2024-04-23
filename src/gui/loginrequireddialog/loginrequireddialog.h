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

#include <QWidget>

namespace Ui {
class LoginRequiredDialog;
}

namespace OCC {

/**
 * This dialog is used to ask users to re-authenticate in case an existing account's credentials no longer work or the user logged out.
 * It is one of two locations in the code where we have users log in to their accounts (the other one is the setup wizard).
 */
class LoginRequiredDialog : public QWidget
{
    Q_OBJECT

public:
    /**
     * Defines which form is shown to the user.
     * The form widgets are already initialized by the content widget (a stacked widget), we just need to bring the right one to the front.
     */
    enum class Mode {
        Basic,
        OAuth,
    };

    explicit LoginRequiredDialog(Mode mode, QWidget *parent = nullptr);
    ~LoginRequiredDialog() override;

    void setTopLabelText(const QString &newText);

    /**
     * Form widget currently shown to the user.
     * @return form widget
     */
    [[nodiscard]] QWidget *contentWidget() const;

private:
    ::Ui::LoginRequiredDialog *_ui;
};

} // OCC

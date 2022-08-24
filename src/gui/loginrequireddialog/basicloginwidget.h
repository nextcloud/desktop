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

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace Ui {
class BasicLoginWidget;
}

namespace OCC {

/**
 * Form-style widget used to log in to accounts using HTTP Basic auth.
 * Used by the login required dialog as well as the setup wizard.
 */
class BasicLoginWidget : public AbstractLoginWidget
{
    Q_OBJECT

public:
    explicit BasicLoginWidget(QWidget *parent = nullptr);
    ~BasicLoginWidget() override;

    /**
     * Enter provided username into line edit and lock it from user interaction.
     * For use primarily with WebFinger.
     * @param username username to use
     */
    void forceUsername(const QString &username);

    QString username();
    QString password();

private:
    ::Ui::BasicLoginWidget *_ui;
};

} // OCC

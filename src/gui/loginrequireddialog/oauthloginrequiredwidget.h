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

#include "abstractloginrequiredwidget.h"
#include "account.h"

#include <QPushButton>
#include <QWidget>

namespace Ui {
class OAuthLoginRequiredWidget;
}

namespace OCC {

class OAuthLoginRequiredWidget : public AbstractLoginRequiredWidget
{
    Q_OBJECT

public:
    explicit OAuthLoginRequiredWidget(AccountPtr accountPtr, QWidget *parent = nullptr);
    ~OAuthLoginRequiredWidget() override;

    QList<QPair<QAbstractButton *, QDialogButtonBox::ButtonRole>> buttons() override;

private:
    ::Ui::OAuthLoginRequiredWidget *_ui;

    QPushButton *_openBrowserButton;
    QPushButton *_copyURLToClipboardButton;
};

}

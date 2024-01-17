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
#include <QPushButton>
#include <QWidget>

namespace Ui {
class OAuthLoginWidget;
}

namespace OCC {

/**
 * Used to log in to OAuth accounts, e.g., when the user logged out, or the authorization token timed out.
 * Used by the login required dialog as well as the setup wizard.
 */
class OAuthLoginWidget : public AbstractLoginWidget
{
    Q_OBJECT

public:
    explicit OAuthLoginWidget(QWidget *parent = nullptr);
    ~OAuthLoginWidget() override;

    void setOpenBrowserButtonText(const QString &newText);

    void showRetryFrame();
    void hideRetryFrame();

    void setUrl(const QUrl &url);
    QUrl url();

Q_SIGNALS:
    void retryButtonClicked();

Q_SIGNALS:
    void openBrowserButtonClicked(const QUrl &url);

private:
    ::Ui::OAuthLoginWidget *_ui;
    QUrl _url;
};

}

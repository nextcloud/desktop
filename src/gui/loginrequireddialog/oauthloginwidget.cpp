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

#include "oauthloginwidget.h"
#include "ui_oauthloginwidget.h"

#include "gui/application.h"
#include "gui/creds/httpcredentialsgui.h"
#include "gui/guiutility.h"

#include "resources/resources.h"

#include <QClipboard>

namespace OCC {

OAuthLoginWidget::OAuthLoginWidget(QWidget *parent)
    : AbstractLoginWidget(parent)
    , _ui(new ::Ui::OAuthLoginWidget)
{
    _ui->setupUi(this);


    _ui->openBrowserButton->setDisabled(true);
    _ui->copyUrlToClipboardButton->setDisabled(true);
    connect(_ui->openBrowserButton, &QPushButton::clicked, this, [this] {
        Q_ASSERT(_url.isValid());
        Q_EMIT openBrowserButtonClicked(_url);
    });
    connect(_ui->copyUrlToClipboardButton, &QPushButton::clicked, this, [this] {
        Q_ASSERT(_url.isValid());
        qApp->clipboard()->setText(_url.toString(QUrl::FullyEncoded));
    });

    // depending on the theme we have to use a light or dark icon
    _ui->copyUrlToClipboardButton->setIcon(Resources::getCoreIcon(QStringLiteral("copy")));

    setFocusProxy(_ui->openBrowserButton);

    connect(_ui->retryButton, &QPushButton::clicked, this, [this]() {
        Q_EMIT retryButtonClicked();
    });

    hideRetryFrame();
}

OAuthLoginWidget::~OAuthLoginWidget()
{
    delete _ui;
}

void OAuthLoginWidget::setOpenBrowserButtonText(const QString &newText)
{
    _ui->openBrowserButton->setText(newText);
}

void OAuthLoginWidget::showRetryFrame()
{
    _ui->retryWidget->setCurrentWidget(_ui->retryPage);

    _ui->openBrowserButton->setEnabled(false);
    _ui->copyUrlToClipboardButton->setEnabled(false);
}

void OAuthLoginWidget::hideRetryFrame()
{
    _ui->retryWidget->setCurrentWidget(_ui->emptyPage);
}

QUrl OAuthLoginWidget::url()
{
    return _url;
}

void OAuthLoginWidget::setUrl(const QUrl &url)
{
    _url = url;
    _ui->openBrowserButton->setEnabled(true);
    _ui->copyUrlToClipboardButton->setEnabled(true);
}
}

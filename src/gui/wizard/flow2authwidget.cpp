/*
 * Copyright (C) by Michael Schuster <michael@schuster.ms>
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

#include "flow2authwidget.h"

#include "common/utility.h"
#include "account.h"
#include "wizard/owncloudwizardcommon.h"

#include "QProgressIndicator.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcFlow2AuthWidget, "nextcloud.gui.wizard.flow2authwidget", QtInfoMsg)


Flow2AuthWidget::Flow2AuthWidget(QWidget *parent)
    : QWidget(parent)
    , _progressIndi(new QProgressIndicator(this))
{
    _ui.setupUi(this);

    WizardCommon::initErrorLabel(_ui.errorLabel);
    _ui.errorLabel->setTextFormat(Qt::RichText);

    connect(_ui.openLinkButton, &QCommandLinkButton::clicked, this, &Flow2AuthWidget::slotOpenBrowser);
    connect(_ui.copyLinkButton, &QCommandLinkButton::clicked, this, &Flow2AuthWidget::slotCopyLinkToClipboard);

    _ui.horizontalLayout->addWidget(_progressIndi);
    stopSpinner(false);

    customizeStyle();
}

void Flow2AuthWidget::startAuth(Account *account)
{
    Flow2Auth *oldAuth = _asyncAuth.take();
    if(oldAuth)
        oldAuth->deleteLater();

    _statusUpdateSkipCount = 0;

    if(account) {
        _account = account;

        _asyncAuth.reset(new Flow2Auth(_account, this));
        connect(_asyncAuth.data(), &Flow2Auth::result, this, &Flow2AuthWidget::slotAuthResult, Qt::QueuedConnection);
        connect(_asyncAuth.data(), &Flow2Auth::statusChanged, this, &Flow2AuthWidget::slotStatusChanged);
        connect(this, &Flow2AuthWidget::pollNow, _asyncAuth.data(), &Flow2Auth::slotPollNow);
        _asyncAuth->start();
    }
}

void Flow2AuthWidget::resetAuth(Account *account)
{
    startAuth(account);
}

void Flow2AuthWidget::slotAuthResult(Flow2Auth::Result r, const QString &errorString, const QString &user, const QString &appPassword)
{
    stopSpinner(false);

    switch (r) {
    case Flow2Auth::NotSupported:
        /* Flow2Auth can't open browser */
        _ui.errorLabel->setText(tr("Unable to open the Browser, please copy the link to your Browser."));
        _ui.errorLabel->show();
        break;
    case Flow2Auth::Error:
        /* Error while getting the access token.  (Timeout, or the server did not accept our client credentials */
        _ui.errorLabel->setText(errorString);
        _ui.errorLabel->show();
        break;
    case Flow2Auth::LoggedIn: {
        _ui.errorLabel->hide();
        break;
    }
    }

    emit authResult(r, errorString, user, appPassword);
}

void Flow2AuthWidget::setError(const QString &error) {
    if (error.isEmpty()) {
        _ui.errorLabel->hide();
    } else {
        _ui.errorLabel->setText(error);
        _ui.errorLabel->show();
    }
}

Flow2AuthWidget::~Flow2AuthWidget() {
    // Forget sensitive data
    _asyncAuth.reset();
}

void Flow2AuthWidget::slotOpenBrowser()
{
    if (_ui.errorLabel)
        _ui.errorLabel->hide();

    if (_asyncAuth)
        _asyncAuth->openBrowser();
}

void Flow2AuthWidget::slotCopyLinkToClipboard()
{
    if (_ui.errorLabel)
        _ui.errorLabel->hide();

    if (_asyncAuth)
        _asyncAuth->copyLinkToClipboard();
}

void Flow2AuthWidget::slotPollNow()
{
    emit pollNow();
}

void Flow2AuthWidget::slotStatusChanged(Flow2Auth::PollStatus status, int secondsLeft)
{
    switch(status)
    {
    case Flow2Auth::statusPollCountdown:
        if(_statusUpdateSkipCount > 0) {
            _statusUpdateSkipCount--;
            break;
        }
        _ui.statusLabel->setText(tr("Waiting for authorization") + QString("… (%1)").arg(secondsLeft));
        stopSpinner(true);
        break;
    case Flow2Auth::statusPollNow:
        _statusUpdateSkipCount = 0;
        _ui.statusLabel->setText(tr("Polling for authorization") + "…");
        startSpinner();
        break;
    case Flow2Auth::statusFetchToken:
        _statusUpdateSkipCount = 0;
        _ui.statusLabel->setText(tr("Starting authorization") + "…");
        startSpinner();
        break;
    case Flow2Auth::statusCopyLinkToClipboard:
        _ui.statusLabel->setText(tr("Link copied to clipboard."));
        _statusUpdateSkipCount = 3;
        stopSpinner(true);
        break;
    }
}

void Flow2AuthWidget::startSpinner()
{
    _ui.horizontalLayout->setEnabled(true);
    _ui.statusLabel->setVisible(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();

    _ui.openLinkButton->setEnabled(false);
    _ui.copyLinkButton->setEnabled(false);
}

void Flow2AuthWidget::stopSpinner(bool showStatusLabel)
{
    _ui.horizontalLayout->setEnabled(false);
    _ui.statusLabel->setVisible(showStatusLabel);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();

    _ui.openLinkButton->setEnabled(_statusUpdateSkipCount == 0);
    _ui.copyLinkButton->setEnabled(_statusUpdateSkipCount == 0);
}

void Flow2AuthWidget::slotStyleChanged()
{
    customizeStyle();
}

void Flow2AuthWidget::customizeStyle()
{
    if(_progressIndi)
        _progressIndi->setColor(QGuiApplication::palette().color(QPalette::Text));
}

} // namespace OCC

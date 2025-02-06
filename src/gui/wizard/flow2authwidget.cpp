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


#include "account.h"
#include "buttonstyle.h"
#include "common/utility.h"
#include "creds/webflowcredentials.h"
#include "linklabel.h"
#include "networkjobs.h"
#include "theme.h"
#include "wizard/owncloudwizardcommon.h"

#include "QProgressIndicator.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringLiteral>

namespace OCC {

Q_LOGGING_CATEGORY(lcFlow2AuthWidget, "hidrivenext.gui.wizard.flow2authwidget", QtInfoMsg)


Flow2AuthWidget::Flow2AuthWidget(QWidget *parent)
    : QWidget(parent)
    , _progressIndi(new QProgressIndicator(this))
{
    _ui.setupUi(this);

    _ui.errorSnackbar->setVisible(false);

    connect(_ui.openLinkButton, &QPushButton::clicked, this, &Flow2AuthWidget::slotOpenBrowser);
    connect(_ui.copyLinkButton, &QPushButton::clicked, this, &Flow2AuthWidget::slotCopyLinkToClipboard);

    auto sizePolicy = _progressIndi->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    _progressIndi->setSizePolicy(sizePolicy);

    _ui.progressLayout->addWidget(_progressIndi);
    stopSpinner(false);

    customizeStyle();
}

void Flow2AuthWidget::setLogo()
{
    const auto logoIconFileName = Theme::hidpiFileName(":/client/theme/ses/ses-external.svg");
    _ui.logoLabel->setPixmap(logoIconFileName);
}

void Flow2AuthWidget::startAuth(Account *account)
{
    const auto oldAuth = _asyncAuth.release();
    if (oldAuth) {
        oldAuth->deleteLater();
    }

    _statusUpdateSkipCount = 0;

    if(account) {
        _account = account;

        _asyncAuth = std::make_unique<Flow2Auth>(_account, this);
        connect(_asyncAuth.get(), &Flow2Auth::result, this, &Flow2AuthWidget::slotAuthResult, Qt::QueuedConnection);
        connect(_asyncAuth.get(), &Flow2Auth::statusChanged, this, &Flow2AuthWidget::slotStatusChanged);
        connect(this, &Flow2AuthWidget::pollNow, _asyncAuth.get(), &Flow2Auth::slotPollNow);
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
        setError(tr("Error"),tr("Unable to open the Browser, please copy the link to your Browser."));
        break;
    case Flow2Auth::Error:
        /* Error while getting the access token.  (Timeout, or the server did not accept our client credentials */
        setError(tr("Error"), errorString);
        break;
    case Flow2Auth::LoggedIn: {
        _ui.errorSnackbar->hide();
        break;
    }
    }

    emit authResult(r, errorString, user, appPassword);
}

void Flow2AuthWidget::setError(const QString &caption, const QString &message) {
    if (message.isEmpty()) {
        _ui.errorSnackbar->hide();
    } else {
        _ui.errorSnackbar->setError(message);
        _ui.errorSnackbar->show();
    }
}

Flow2AuthWidget::~Flow2AuthWidget() {
    // Forget sensitive data
    _asyncAuth.reset(nullptr);
}

void Flow2AuthWidget::slotOpenBrowser()
{
    if (_ui.errorSnackbar)
        _ui.errorSnackbar->hide();

    if (_asyncAuth)
        _asyncAuth->openBrowser();
}

void Flow2AuthWidget::slotCopyLinkToClipboard()
{
    if (_ui.errorSnackbar)
        _ui.errorSnackbar->hide();

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
        _ui.statusLabel->setText(tr("Waiting for authorization") + QStringLiteral("… (%1)").arg(secondsLeft));
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
    _ui.progressLayout->setEnabled(true);
    _ui.statusLabel->setVisible(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();

    _ui.openLinkButton->setEnabled(false);
    _ui.copyLinkButton->setEnabled(false);
}

void Flow2AuthWidget::stopSpinner(bool showStatusLabel)
{
    _ui.progressLayout->setEnabled(false);
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

void Flow2AuthWidget::shrinkTopMarginForText()
{
    _ui.topMarginSpacer->changeSize(20, 30);
    _ui.topMarginSpacer->invalidate();
    setMinimumHeight(340);
    setMaximumHeight(400);
}

void Flow2AuthWidget::customizeStyle()
{
    setLogo();

    if (_progressIndi) {
        const auto isDarkBackground = Theme::isDarkColor(palette().window().color());
        if (isDarkBackground) {
            _progressIndi->setColor(Qt::white);
        } else {
            _progressIndi->setColor(Qt::black);
        }
    }

    _ui.openLinkButton->setText(tr("Open Browser"));
    _ui.openLinkButton->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));
    _ui.openLinkButton->setFixedSize(180, 40);

    _ui.copyLinkButton->setText(tr("Copy Link"));
    _ui.copyLinkButton->setFixedSize(150, 40);

    _ui.mainLayoutVBox->setContentsMargins(32, 0, 32, 0);
    _ui.innerLayoutVBox->setSpacing(16);
    
#ifdef Q_OS_MAC
    _ui.horizontalLayout->setSpacing(32);
#endif

    _ui.statusLabel->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::titleColor()
    ));

    _ui.label->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::titleColor()
    ));

    _ui.label->setText(tr("Switch to your browser to connect your account"));
}

} // namespace OCC

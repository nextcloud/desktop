/*
 * Copyright (C) by Jyrki Gadinger <nilsding@nilsding.org>
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

#include "termsofservicecheckwidget.h"

#include "wizard/owncloudwizardcommon.h"
#include "theme.h"
#include "configfile.h"

#include "QProgressIndicator.h"

#include <QClipboard>
#include <QDesktopServices>

namespace OCC {

Q_LOGGING_CATEGORY(lcTosCheckWidget, "nextcloud.gui.wizard.termsofservicecheckwidget", QtInfoMsg)


TermsOfServiceCheckWidget::TermsOfServiceCheckWidget(QWidget *parent)
    : QWidget(parent)
    , _progressIndicator(new QProgressIndicator(this))
{
    _pollTimer.setInterval(1000);
    QObject::connect(&_pollTimer, &QTimer::timeout, this, &TermsOfServiceCheckWidget::slotPollTimerTimeout);

    _ui.setupUi(this);

    connect(_ui.openLinkButton, &QPushButton::clicked, this, &TermsOfServiceCheckWidget::slotOpenBrowser);
    connect(_ui.copyLinkButton, &QPushButton::clicked, this, &TermsOfServiceCheckWidget::slotCopyLinkToClipboard);

    auto sizePolicy = _progressIndicator->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    _progressIndicator->setSizePolicy(sizePolicy);

    _ui.progressLayout->addWidget(_progressIndicator);
    stopSpinner(false);

    customizeStyle();
}

TermsOfServiceCheckWidget::~TermsOfServiceCheckWidget() {
}

void TermsOfServiceCheckWidget::start()
{
    ConfigFile cfg;
    std::chrono::milliseconds polltime = cfg.remotePollInterval();
    qCInfo(lcTosCheckWidget) << "setting remote poll timer interval to" << polltime.count() << "msec";
    _secondsInterval = (polltime.count() / 1000);
    _secondsLeft = _secondsInterval;

    _pollTimer.start();
    // open browser when the wizard page is shown
    slotOpenBrowser();
}

void TermsOfServiceCheckWidget::setUrl(const QUrl &url)
{
    _url = url;
}

void TermsOfServiceCheckWidget::termsNotAcceptedYet()
{
    _secondsLeft = _secondsInterval;
    _isBusy = false;
    statusChanged(Status::statusPollCountdown);
}

void TermsOfServiceCheckWidget::setLogo()
{
    const auto backgroundColor = palette().window().color();
    const auto logoIconFileName = Theme::instance()->isBranded() ? Theme::hidpiFileName("external.png", backgroundColor)
                                                                 : Theme::hidpiFileName(":/client/theme/colored/external.png");
    _ui.logoLabel->setPixmap(logoIconFileName);
}

void TermsOfServiceCheckWidget::slotStyleChanged()
{
    customizeStyle();
}

void TermsOfServiceCheckWidget::slotPollTimerTimeout()
{
    if (_isBusy) {
        return;
    }

    _isBusy = true;

    _secondsLeft--;
    if (_secondsLeft > 0) {
        statusChanged(Status::statusPollCountdown);
        _isBusy = false;
        return;
    }

    statusChanged(Status::statusPollNow);
    Q_EMIT pollNow();
}

void TermsOfServiceCheckWidget::slotOpenBrowser()
{
    QDesktopServices::openUrl(_url);
}

void TermsOfServiceCheckWidget::slotCopyLinkToClipboard()
{
    statusChanged(Status::statusCopyLinkToClipboard);
    QApplication::clipboard()->setText(_url.toString(QUrl::FullyEncoded));
}

void TermsOfServiceCheckWidget::statusChanged(Status status)
{
    switch (status)
    {
    case statusPollCountdown:
        if (_statusUpdateSkipCount > 0) {
            _statusUpdateSkipCount--;
            return;
        }

        _ui.statusLabel->setText(tr("Waiting for terms to be accepted") + QStringLiteral("… (%1)").arg(_secondsLeft));
        stopSpinner(true);
        return;

    case statusPollNow:
        _statusUpdateSkipCount = 0;
        _ui.statusLabel->setText(tr("Polling") + QStringLiteral("…"));
        startSpinner();
        return;

    case statusCopyLinkToClipboard:
        _statusUpdateSkipCount = 3;
        _ui.statusLabel->setText(tr("Link copied to clipboard."));
        stopSpinner(true);
        return;
    }
}

void TermsOfServiceCheckWidget::startSpinner()
{
    _ui.progressLayout->setEnabled(true);
    _ui.statusLabel->setVisible(true);
    _progressIndicator->setVisible(true);
    _progressIndicator->startAnimation();

    _ui.openLinkButton->setEnabled(false);
    _ui.copyLinkButton->setEnabled(false);
}

void TermsOfServiceCheckWidget::stopSpinner(bool showStatusLabel)
{
    _ui.progressLayout->setEnabled(false);
    _ui.statusLabel->setVisible(showStatusLabel);
    _progressIndicator->setVisible(false);
    _progressIndicator->stopAnimation();

    _ui.openLinkButton->setEnabled(_statusUpdateSkipCount == 0);
    _ui.copyLinkButton->setEnabled(_statusUpdateSkipCount == 0);
}

void TermsOfServiceCheckWidget::customizeStyle()
{
    setLogo();

    if (_progressIndicator) {
        const auto isDarkBackground = Theme::isDarkColor(palette().window().color());
        if (isDarkBackground) {
            _progressIndicator->setColor(Qt::white);
        } else {
            _progressIndicator->setColor(Qt::black);
        }
    }

    _ui.openLinkButton->setText(tr("Open Browser"));

    _ui.copyLinkButton->setText(tr("Copy Link"));

    WizardCommon::customizeHintLabel(_ui.statusLabel);
}

} // namespace OCC

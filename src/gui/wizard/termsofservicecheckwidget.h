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

#pragma once

#include <QWidget>
#include <QTimer>

#include "ui_termsofservicecheckwidget.h"

class QProgressIndicator;

namespace OCC {

class TermsOfServiceCheckWidget : public QWidget
{
    Q_OBJECT
public:
    enum Status {
        statusPollCountdown = 1,
        statusPollNow,
        statusCopyLinkToClipboard,
    };

    TermsOfServiceCheckWidget(QWidget *parent = nullptr);
    ~TermsOfServiceCheckWidget() override;

    void start();
    void setUrl(const QUrl &url);
    void termsNotAcceptedYet();

public Q_SLOTS:
    void slotStyleChanged();

Q_SIGNALS:
    void pollNow();

private Q_SLOTS:
    void slotPollTimerTimeout();
    void slotOpenBrowser();
    void slotCopyLinkToClipboard();

private:
    Ui_TermsOfServiceCheckWidget _ui{};
    QTimer _pollTimer;
    QProgressIndicator *_progressIndicator = nullptr;
    int _statusUpdateSkipCount = 0;
    qint64 _secondsLeft = 0LL;
    qint64 _secondsInterval = 0LL;
    bool _isBusy = false;
    QUrl _url;

    void statusChanged(Status status);
    void startSpinner();
    void stopSpinner(bool showStatusLabel);
    void customizeStyle();
    void setLogo();

};

} // namespace OCC

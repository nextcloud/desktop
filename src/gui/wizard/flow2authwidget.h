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

#ifndef FLOW2AUTHWIDGET_H
#define FLOW2AUTHWIDGET_H

#include <QUrl>
#include <QWidget>

#include "creds/flow2auth.h"

#include "ui_flow2authwidget.h"

class QProgressIndicator;

namespace OCC {

class Flow2AuthWidget : public QWidget
{
    Q_OBJECT
public:
    Flow2AuthWidget(QWidget *parent = nullptr);
    ~Flow2AuthWidget() override;

    void startAuth(Account *account);
    void resetAuth(Account *account = nullptr);
    void setError(const QString &error);

public Q_SLOTS:
    void slotAuthResult(Flow2Auth::Result, const QString &errorString, const QString &user, const QString &appPassword);
    void slotPollNow();
    void slotStatusChanged(Flow2Auth::PollStatus status, int secondsLeft);
    void slotStyleChanged();

Q_SIGNALS:
    void authResult(Flow2Auth::Result, const QString &errorString, const QString &user, const QString &appPassword);
    void pollNow();

private:
    Account *_account = nullptr;
    std::unique_ptr<Flow2Auth> _asyncAuth;
    Ui_Flow2AuthWidget _ui{};

protected Q_SLOTS:
    void slotOpenBrowser();
    void slotCopyLinkToClipboard();

private:
    void startSpinner();
    void stopSpinner(bool showStatusLabel);
    void customizeStyle();
    void setLogo();

    QProgressIndicator *_progressIndi;
    int _statusUpdateSkipCount = 0;
};

} // namespace OCC

#endif // FLOW2AUTHWIDGET_H

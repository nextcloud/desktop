/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 * Copyright (C) by Michael Schuster <michael@nextcloud.com>
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

#include <QList>
#include <QMap>
#include <QNetworkCookie>
#include <QUrl>
#include <QPointer>

#include "wizard/abstractcredswizardpage.h"
#include "accountfwd.h"
#include "creds/flow2auth.h"

#include "ui_flow2authcredspage.h"

class QProgressIndicator;

namespace OCC {

class Flow2AuthCredsPage : public AbstractCredentialsWizardPage
{
    Q_OBJECT
public:
    Flow2AuthCredsPage();

    AbstractCredentials *getCredentials() const override;

    void initializePage() override;
    void cleanupPage() override;
    int nextId() const override;
    void setConnected();
    bool isComplete() const override;

public Q_SLOTS:
    void asyncAuthResult(Flow2Auth::Result, const QString &user, const QString &appPassword);
    void slotPollNow();
    void slotStatusChanged(int secondsLeft);
    void slotStyleChanged();

signals:
    void connectToOCUrl(const QString &);
    void pollNow();

public:
    QString _user;
    QString _appPassword;
    QScopedPointer<Flow2Auth> _asyncAuth;
    Ui_Flow2AuthCredsPage _ui;

protected slots:
    void slotOpenBrowser();
    void slotCopyLinkToClipboard();

private:
    void startSpinner();
    void stopSpinner(bool showStatusLabel);
    void customizeStyle();

    QProgressIndicator *_progressIndi;
};

} // namespace OCC

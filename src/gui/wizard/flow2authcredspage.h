/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#pragma once

#include <QList>
#include <QMap>
#include <QNetworkCookie>
#include <QUrl>
#include <QPointer>

#include "wizard/abstractcredswizardpage.h"
#include "accountfwd.h"
#include "creds/flow2auth.h"

class QVBoxLayout;
class QProgressIndicator;

namespace OCC {

class Flow2AuthWidget;

class Flow2AuthCredsPage : public AbstractCredentialsWizardPage
{
    Q_OBJECT
public:
    Flow2AuthCredsPage();

    [[nodiscard]] AbstractCredentials *getCredentials() const override;

    void initializePage() override;
    void cleanupPage() override;
    [[nodiscard]] int nextId() const override;
    void setConnected();
    [[nodiscard]] bool isComplete() const override;

public Q_SLOTS:
    void slotFlow2AuthResult(Flow2Auth::Result, const QString &errorString, const QString &user, const QString &appPassword);
    void slotPollNow();
    void slotStyleChanged();

Q_SIGNALS:
    void connectToOCUrl(const QString &);
    void pollNow();
    void styleChanged();

public:
    QString _user;
    QString _appPassword;

private:
    Flow2AuthWidget *_flow2AuthWidget = nullptr;
    QVBoxLayout *_layout = nullptr;
};

} // namespace OCC

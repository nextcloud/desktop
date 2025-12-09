/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    void setConnected();
    [[nodiscard]] bool isComplete() const override;

public Q_SLOTS:
    void slotFlow2AuthResult(OCC::Flow2Auth::Result, const QString &errorString, const QString &user, const QString &appPassword);
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

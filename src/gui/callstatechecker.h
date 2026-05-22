/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QTimer>
#include <qqmlregistration.h>

#include "networkjobs.h"
#include "accountstate.h"

namespace OCC {

class CallStateChecker : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString token READ token WRITE setToken NOTIFY tokenChanged)
    Q_PROPERTY(AccountState* accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)
    Q_PROPERTY(bool checking READ checking WRITE setChecking NOTIFY checkingChanged)

public:
    explicit CallStateChecker(QObject *parent = nullptr);

    [[nodiscard]] QString token() const;
    [[nodiscard]] AccountState* accountState() const;
    [[nodiscard]] bool checking() const;

signals:
    void tokenChanged();
    void accountStateChanged();
    void checkingChanged();

    void stopNotifying();

public slots:
    void setToken(const QString &token);
    void setAccountState(OCC::AccountState *accountState);
    void setChecking(const bool checking);

private slots:
    void slotStatusCheckTimerElapsed();
    void slotNotificationTimerElapsed();
    void slotCallStateReceived(const QJsonDocument &json, const int statusCode);
    void reset();

private:
    void setup();
    void startCallStateCheck();
    [[nodiscard]] bool isAccountServerVersion22OrLater() const;

    AccountState *_accountState = nullptr;
    QString _token;
    QTimer _statusCheckTimer;   // How often we check the status of the call
    QTimer _notificationTimer;  // How long we present the call notification for
    QPointer<JsonApiJob> _stateCheckJob;
    bool _checking = false;
};

}

/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QPointer>

#include "gui/accountstate.h"
#include "libsync/syncresult.h"

class QLocalSocket;

namespace OCC {

namespace Mac {

class FileProviderSocketController : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderSocketController(QLocalSocket * const socket, QObject * const parent = nullptr);

    [[nodiscard]] AccountStatePtr accountState() const;
    
signals:
    void socketDestroyed(const QLocalSocket * const socket);
    void syncStateChanged(const AccountPtr &account, SyncResult::Status state) const;

public slots:
    void sendMessage(const QString &message) const;
    void start();

private slots:
    void slotOnDisconnected();
    void slotSocketDestroyed(const QObject * const object);
    void slotReadyRead();

    void slotAccountStateChanged(const OCC::AccountState::State state) const;

    void parseReceivedLine(const QString &receivedLine);
    void requestFileProviderDomainInfo() const;
    void sendAccountDetails() const;
    void sendNotAuthenticated() const;
    void sendIgnoreList() const;

    void reportSyncState(const QString &receivedState) const;

private:
    QPointer<QLocalSocket> _socket;
    AccountStatePtr _accountState;
};

} // namespace Mac

} // namespace OCC

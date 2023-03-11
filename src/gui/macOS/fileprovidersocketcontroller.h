/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include <QObject>
#include <QPointer>
#include <QLocalSocket>

#include "accountstate.h"

namespace OCC {

namespace Mac {

class FileProviderSocketController : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderSocketController(QLocalSocket * const socket, QObject * const parent = nullptr);

signals:
    void socketDestroyed(const QLocalSocket * const socket);

public slots:
    void sendMessage(const QString &message) const;
    void start();

private slots:
    void slotOnDisconnected();
    void slotSocketDestroyed(const QObject * const object);
    void slotReadyRead();

    void slotAccountStateChanged(const AccountState::State state);

    void parseReceivedLine(const QString &receivedLine);
    void requestFileProviderDomainInfo() const;
    void sendAccountDetails() const;

private:
    static AccountStatePtr accountStateFromFileProviderDomainIdentifier(const QString &domainIdentifier);

    QPointer<QLocalSocket> _socket;
    AccountStatePtr _accountState;
};

} // namespace Mac

} // namespace OCC

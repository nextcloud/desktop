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

#include "fileprovidersocketserver.h"

#include <QLocalSocket>
#include <QLoggingCategory>

#include "libsync/account.h"

#include "fileprovidersocketcontroller.h"

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSocketServer, "nextcloud.gui.macos.fileprovider.socketserver", QtInfoMsg)

FileProviderSocketServer::FileProviderSocketServer(QObject *parent)
    : QObject{parent}
{
    _socketPath = fileProviderSocketPath();
    startListening();
}

void FileProviderSocketServer::startListening()
{
    QLocalServer::removeServer(_socketPath);

    const auto serverStarted = _socketServer.listen(_socketPath);
    if (!serverStarted) {
        qCWarning(lcFileProviderSocketServer) << "Could not start file provider socket server"
                                              << _socketPath;
    } else {
        qCInfo(lcFileProviderSocketServer) << "File provider socket server started, listening"
                                           << _socketPath;
    }

    connect(&_socketServer, &QLocalServer::newConnection,
            this, &FileProviderSocketServer::slotNewConnection);
}

void FileProviderSocketServer::slotNewConnection()
{
    if (!_socketServer.hasPendingConnections()) {
        return;
    }

    qCInfo(lcFileProviderSocketServer) << "New connection in file provider socket server";
    const auto socket = _socketServer.nextPendingConnection();
    if (!socket) {
        return;
    }

    const FileProviderSocketControllerPtr socketController(new FileProviderSocketController(socket, this));
    connect(socketController.data(), &FileProviderSocketController::syncStateChanged,
            this, &FileProviderSocketServer::slotSyncStateChanged);
    connect(socketController.data(), &FileProviderSocketController::socketDestroyed,
            this, &FileProviderSocketServer::slotSocketDestroyed);
    _socketControllers.insert(socket, socketController);

    socketController->start();
}

void FileProviderSocketServer::slotSocketDestroyed(const QLocalSocket * const socket)
{
    const auto socketController = _socketControllers.take(socket);

    if (socketController) {
        const auto rawSocketControllerPtr = socketController.data();
        delete rawSocketControllerPtr;
    }
}

void FileProviderSocketServer::slotSyncStateChanged(const AccountPtr &account, SyncResult::Status state)
{
    Q_ASSERT(account);
    const auto userId = account->userIdAtHostWithPort();
    qCDebug(lcFileProviderSocketServer) << "Received sync state change for account" << userId << "state" << state;
    _latestReceivedSyncStatus.insert(userId, state);
    Q_EMIT syncStateChanged(account, state);
}

SyncResult::Status FileProviderSocketServer::latestReceivedSyncStatusForAccount(const AccountPtr &account) const
{
    Q_ASSERT(account);
    return _latestReceivedSyncStatus.value(account->userIdAtHostWithPort(), SyncResult::Undefined);
}

} // namespace Mac

} // namespace OCC

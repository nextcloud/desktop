/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    qCDebug(lcFileProviderSocketServer) << "Initializing...";

    _socketPath = fileProviderSocketPath();
    startListening();

    qCDebug(lcFileProviderSocketServer) << "Initialized.";
}

void FileProviderSocketServer::startListening()
{
    QLocalServer::removeServer(_socketPath);

    const auto serverStarted = _socketServer.listen(_socketPath);

    if (!serverStarted) {
        qCWarning(lcFileProviderSocketServer) << "Could not start file provider socket server"
                                              << _socketPath
                                              << "Error:" 
                                              << _socketServer.errorString()
                                              << "Error code:" 
                                              << _socketServer.serverError();
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

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

#include "fileprovidersocketmanager.h"

#include <QLocalSocket>

#include "fileprovidersocketcontroller.h"

namespace OCC
{

namespace Mac
{

Q_LOGGING_CATEGORY(lcFileProviderSocketManager, "nextcloud.gui.macos.fileprovider.socketmanager", QtInfoMsg)

FileProviderSocketManager::FileProviderSocketManager(QObject *parent)
    : QObject{parent}
{
#ifdef Q_OS_MACOS
    _socketPath = fileProviderSocketPath();
#endif
    startListening();
}

void FileProviderSocketManager::startListening()
{
    QLocalServer::removeServer(_socketPath);

    const auto serverStarted = _socketServer.listen(_socketPath);
    if (!serverStarted) {
        qCWarning(lcFileProviderSocketManager) << "Could not start file provider socket server"
                                               << _socketPath;
    } else {
        qCInfo(lcFileProviderSocketManager) << "File provider socket server started, listening"
                                            << _socketPath;
    }

    connect(&_socketServer, &QLocalServer::newConnection,
            this, &FileProviderSocketManager::slotNewConnection);
}

void FileProviderSocketManager::slotNewConnection()
{
    if (!_socketServer.hasPendingConnections()) {
        return;
    }

    const auto socket = _socketServer.nextPendingConnection();
    if (!socket) {
        return;
    }

    connect(socket, &QLocalSocket::disconnected,
            this, &FileProviderSocketManager::slotOnDisconnected);
    connect(socket, &QLocalSocket::destroyed,
            this, &FileProviderSocketManager::slotSocketDestroyed);

    const FileProviderSocketControllerPtr controller(new FileProviderSocketController(socket));
    _socketControllers.insert(socket, controller);
}

void FileProviderSocketManager::slotOnDisconnected()
{
    const auto socket = qobject_cast<QLocalSocket *>(sender());
    Q_ASSERT(socket);
    socket->deleteLater();
}

void FileProviderSocketManager::slotSocketDestroyed(QObject *object)
{
    const auto socket = qobject_cast<QLocalSocket *>(object);
    _socketControllers.remove(socket);
}

} // namespace Mac

} // namespace OCC

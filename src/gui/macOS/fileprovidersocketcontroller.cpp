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

#include "fileprovidersocketcontroller.h"

#include <QLoggingCategory>

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSocketController, "nextcloud.gui.macos.fileprovider.socketcontroller", QtInfoMsg)

FileProviderSocketController::FileProviderSocketController(QLocalSocket *socket, QObject *parent)
    : QObject{parent}
    , _socket(socket)
{
    connect(socket, &QLocalSocket::disconnected,
            this, &FileProviderSocketController::slotOnDisconnected);
    connect(socket, &QLocalSocket::destroyed,
            this, &FileProviderSocketController::slotSocketDestroyed);
}

void FileProviderSocketController::slotOnDisconnected()
{
    qCInfo(lcFileProviderSocketController) << "File provider socket disconnected";
    _socket->deleteLater();
    Q_EMIT socketDestroyed(_socket);
}

void FileProviderSocketController::slotSocketDestroyed(QObject *object)
{
    Q_UNUSED(object)
    qCInfo(lcFileProviderSocketController) << "File provider socket object has been destroyed, destroying controller";
    Q_EMIT socketDestroyed(_socket);
}

}

}

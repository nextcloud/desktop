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
    connect(socket, &QLocalSocket::readyRead,
            this, &FileProviderSocketController::slotReadyRead);
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

void FileProviderSocketController::slotReadyRead()
{
    Q_ASSERT(_socket);
    while(_socket->canReadLine()) {
        const QString line = QString::fromUtf8(_socket->readLine().trimmed()).normalized(QString::NormalizationForm_C);
        Q_UNUSED(line);
    }
}

void FileProviderSocketController::sendMessage(const QString &message) const
{
    if (!_socket) {
        qCWarning(lcFileProviderSocketController) << "Not sending message on dead file provider socket:" << message;
        return;
    }

    qCDebug(lcFileProviderSocketController) << "Sending File Provider socket message:" << message;
    const auto lineEndChar = '\n';
    const auto messageToSend = message.endsWith(lineEndChar) ? message : message + lineEndChar;
    const auto bytesToSend = messageToSend.toUtf8();
    const auto sent = _socket->write(bytesToSend);

    if (sent != bytesToSend.length()) {
        qCWarning(lcFileProviderSocketController) << "Could not send all data on file provider socket for:" << message;
    }
}

}

}

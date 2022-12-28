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

#include "accountmanager.h"

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
        qCDebug(lcFileProviderSocketController) << "Received message in file provider socket:" << line;

        parseReceivedLine(line);
    }
}

void FileProviderSocketController::parseReceivedLine(const QString &receivedLine)
{
    if (receivedLine.isEmpty()) {
        qCWarning(lcFileProviderSocketController) << "Received empty line, can't parse.";
        return;
    }

    const auto argPos = receivedLine.indexOf(QLatin1Char(':'));
    if (argPos == -1) {
        qCWarning(lcFileProviderSocketController) << "Received line:"
                                                  << receivedLine
                                                  << "is incorrectly structured. Can't parse.";
        return;
    }

    const auto command = receivedLine.mid(0, argPos);
    const auto argument = receivedLine.mid(argPos + 1);

    if (command == QStringLiteral("FILE_PROVIDER_DOMAIN_IDENTIFIER_REQUEST_REPLY")) {
        _accountState = accountStateFromFileProviderDomainIdentifier(argument);
        return;
    }

    qCWarning(lcFileProviderSocketController) << "Unknown command or reply:" << receivedLine;
}

AccountStatePtr FileProviderSocketController::accountStateFromFileProviderDomainIdentifier(const QString &domainIdentifier)
{
    Q_ASSERT(!domainIdentifier.isEmpty());

    // We use Account's userIdAtHostWithPort() as the file provider domain's identifier in FileProviderDomainManager.
    // We can use this string to get a matching account here.
    const auto accountForReceivedDomainIdentifier = AccountManager::instance()->accountFromUserId(domainIdentifier);
    if (!accountForReceivedDomainIdentifier) {
        qCWarning(lcFileProviderSocketController) << "Could not find account matching user id matching file provider domain identifier:"
                                                  << domainIdentifier;
    }

    return accountForReceivedDomainIdentifier;
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


void FileProviderSocketController::start()
{
    Q_ASSERT(_socket);
    requestFileProviderDomainInfo();
}

void FileProviderSocketController::requestFileProviderDomainInfo() const
{
    Q_ASSERT(_socket);
    const auto requestMessage = QStringLiteral("SEND_FILE_PROVIDER_DOMAIN_IDENTIFIER");
    sendMessage(requestMessage);
}

}

}

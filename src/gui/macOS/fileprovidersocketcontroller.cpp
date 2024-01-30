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

#include <QLocalSocket>
#include <QLoggingCategory>

#include "accountmanager.h"
#include "fileproviderdomainmanager.h"

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSocketController, "nextcloud.gui.macos.fileprovider.socketcontroller", QtInfoMsg)

FileProviderSocketController::FileProviderSocketController(QLocalSocket * const socket, QObject * const parent)
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
}

void FileProviderSocketController::slotSocketDestroyed(const QObject * const object)
{
    Q_UNUSED(object)
    qCInfo(lcFileProviderSocketController) << "File provider socket object has been destroyed, destroying controller";
    Q_EMIT socketDestroyed(_socket);
}

void FileProviderSocketController::slotReadyRead()
{
    Q_ASSERT(_socket);
    if (!_socket) {
        qCWarning(lcFileProviderSocketController) << "Cannot read data on dead socket";
        return;
    }

    while(_socket->canReadLine()) {
        const auto line = QString::fromUtf8(_socket->readLine().trimmed()).normalized(QString::NormalizationForm_C);
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
        _accountState = FileProviderDomainManager::accountStateFromFileProviderDomainIdentifier(argument);
        sendAccountDetails();
        return;
    }

    qCWarning(lcFileProviderSocketController) << "Unknown command or reply:" << receivedLine;
}

void FileProviderSocketController::sendMessage(const QString &message) const
{
    if (!_socket) {
        qCWarning(lcFileProviderSocketController) << "Not sending message on dead file provider socket:" << message;
        return;
    }

    qCInfo(lcFileProviderSocketController) << "Sending File Provider socket message:" << message;
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
    if (!_socket) {
        qCWarning(lcFileProviderSocketController) << "Cannot start communication on dead socket";
        return;
    }

    /*
     * We have a new file provider extension connection. When this happens, we:
     * 1. Request the file provider domain identifier
     * 2. Receive the file provider domain identifier from the extension
     * 3. Send the account details to the extension according to the domain identifier
     */
    requestFileProviderDomainInfo();
}

void FileProviderSocketController::requestFileProviderDomainInfo() const
{
    Q_ASSERT(_socket);
    if (!_socket) {
        qCWarning(lcFileProviderSocketController) << "Cannot request file provider domain data on dead socket";
        return;
    }

    const auto requestMessage = QStringLiteral("SEND_FILE_PROVIDER_DOMAIN_IDENTIFIER");
    sendMessage(requestMessage);
}

void FileProviderSocketController::slotAccountStateChanged(const AccountState::State state)
{
    switch(state) {
    case AccountState::Disconnected:
    case AccountState::ConfigurationError:
    case AccountState::NetworkError:
    case AccountState::ServiceUnavailable:
    case AccountState::MaintenanceMode:
        // Do nothing, File Provider will by itself figure out connection issue
        break;
    case AccountState::SignedOut:
    case AccountState::AskingCredentials:
    case AccountState::RedirectDetected:
        // Notify File Provider that it should show the not authenticated message
        sendNotAuthenticated();
        break;
    case AccountState::Connected:
        // Provide credentials
        sendAccountDetails();
        break;
    }
}

void FileProviderSocketController::sendNotAuthenticated() const
{
    Q_ASSERT(_accountState);
    const auto account = _accountState->account();
    Q_ASSERT(account);

    qCDebug(lcFileProviderSocketController) << "About to send not authenticated message to file provider extension"
                                            << account->displayName();

    const auto message = QString(QStringLiteral("ACCOUNT_NOT_AUTHENTICATED"));
    sendMessage(message);
}

void FileProviderSocketController::sendAccountDetails() const
{
    Q_ASSERT(_accountState);
    const auto account = _accountState->account();
    Q_ASSERT(account);

    qCInfo(lcFileProviderSocketController) << "About to send account details to file provider extension"
                                           << account->displayName();

    // Even though we have XPC send over the account details and related calls when the account state changes, in the
    // brief window where we start the file provider extension on app startup and the account state changes, we need to
    // be able to send over the details when the account is done getting configured.
    connect(_accountState.data(), &AccountState::stateChanged,
            this, &FileProviderSocketController::slotAccountStateChanged, Qt::UniqueConnection);

    if (!_accountState->isConnected()) {
        qCWarning(lcFileProviderSocketController) << "Not sending account details yet as account is not connected"
                                                  << account->displayName();
        return;
    }

    const auto credentials = account->credentials();
    Q_ASSERT(credentials);
    const auto accountUser = credentials->user();
    const auto accountUrl = account->url().toString();
    const auto accountPassword = credentials->password();

    // We cannot use colons as separators here due to "https://" in the url
    const auto message = QString(QStringLiteral("ACCOUNT_DETAILS:") +
                                 accountUser + "~" +
                                 accountUrl + "~" +
                                 accountPassword);
    sendMessage(message);
}

}

}

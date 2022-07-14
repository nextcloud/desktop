/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "thumbnailprovideripc.h"
#include "common/cfapishellextensionsipcconstants.h"
#include <QString>
#include <QSize>
#include <QtNetwork/QLocalSocket>
#include <QJsonDocument>
#include <QObject>
#include <Windows.h>
namespace {
// we don't want to block the Explorer for too long (default is 30K, so we'd keep it at 10K, except QLocalSocket::waitForDisconnected())
constexpr auto socketTimeoutMs = 10000;
}

namespace CfApiShellExtensions {

ThumbnailProviderIpc::ThumbnailProviderIpc()
{
    MessageBox(NULL, L"TEST", L"TEST", MB_OK);
    _localSocket = new QLocalSocket();
    auto isOpen = _localSocket->isOpen();
}
ThumbnailProviderIpc::~ThumbnailProviderIpc()
{
    disconnectSocketFromServer();
    _localSocket->deleteLater();
}

QByteArray ThumbnailProviderIpc::fetchThumbnailForFile(const QString &filePath, const QSize &size)
{
    QByteArray result;
    const auto sendMessageAndReadyRead = [this](const QByteArray &message) {
        _localSocket->write(message);
        return _localSocket->waitForBytesWritten(socketTimeoutMs) && _localSocket->waitForReadyRead(socketTimeoutMs);
    };

    // #1 Connect to the main server and send a request for a thumbnail
    if (!connectSocketToServer(CfApiShellExtensions::ThumbnailProviderMainServerName)) {
        return result;
    }

    // send the file path so the main server will decide which sync root we are working with
    const auto messageRequestThumbnailForFile = QJsonDocument::fromVariant(
        QVariantMap{{CfApiShellExtensions::Protocol::ThumbnailProviderRequestKey,
            QVariantMap{{CfApiShellExtensions::Protocol::ThumbnailProviderRequestFilePathKey, filePath},
                {CfApiShellExtensions::Protocol::ThumbnailProviderRequestFileSizeKey,
                    QVariantMap{{"x", size.width()},
                        {"y", size.height()}}}}}}).toJson(QJsonDocument::Compact);

    if (!sendMessageAndReadyRead(messageRequestThumbnailForFile)) {
        return result;
    }

    // #2 Get the name of a server for the current syncroot
    const auto receivedSyncrootServerNameMessage = QJsonDocument::fromJson(_localSocket->readAll()).toVariant().toMap();
    const auto serverNameReceived =
        receivedSyncrootServerNameMessage.value(CfApiShellExtensions::Protocol::ThumbnailProviderServerNameKey)
            .toString();

    if (serverNameReceived.isEmpty()) {
        disconnectSocketFromServer();
        return result;
    }

    // #3 Connect to the current syncroot folder's server
    if (!connectSocketToServer(serverNameReceived)) {
        return result;
    }

    // #4 Request a thumbnail of size (x, y) for a file _shellItemPath
    if (!sendMessageAndReadyRead(messageRequestThumbnailForFile)) {
        return result;
    }

    // #5 Read the thumbnail data from the current syncroot folder's server (read all as the thumbnail size is usually
    // less than 1MB)
    result = _localSocket->readAll();
    disconnectSocketFromServer();

    return result;
}

bool ThumbnailProviderIpc::disconnectSocketFromServer()
{
    const auto isConnectedOrConnecting =
        _localSocket->state() == QLocalSocket::ConnectedState || _localSocket->state() == QLocalSocket::ConnectingState;
    if (isConnectedOrConnecting) {
        _localSocket->disconnectFromServer();
        const auto isNotConnected = _localSocket->state() == QLocalSocket::UnconnectedState
            || _localSocket->state() == QLocalSocket::ClosingState;
        return isNotConnected || _localSocket->waitForDisconnected();
    }
    return true;
}

bool ThumbnailProviderIpc::connectSocketToServer(const QString &serverName)
{
    if (!disconnectSocketFromServer()) {
        return false;
    }
    _localSocket->setServerName(serverName);
    _localSocket->connectToServer();
    return _localSocket->state() == QLocalSocket::ConnectedState || _localSocket->waitForConnected(socketTimeoutMs);
}
}
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
#include "common/shellextensionutils.h"
#include "ipccommon.h"
#include <QString>
#include <QSize>
#include <QtNetwork/QLocalSocket>
#include <QJsonDocument>
namespace {
// we don't want to block the Explorer for too long (default is 30K, so we'd keep it at 10K, except QLocalSocket::waitForDisconnected())
constexpr auto socketTimeoutMs = 10000;
}

namespace VfsShellExtensions {

ThumbnailProviderIpc::ThumbnailProviderIpc()
{
    _localSocket.reset(new QLocalSocket());
}
ThumbnailProviderIpc::~ThumbnailProviderIpc()
{
    disconnectSocketFromServer();
}

QByteArray ThumbnailProviderIpc::fetchThumbnailForFile(const QString &filePath, const QSize &size)
{
    QByteArray result;
    const auto sendMessageAndReadyRead = [this](QVariantMap &message) {
        _localSocket->write(VfsShellExtensions::Protocol::createJsonMessage(message));
        return _localSocket->waitForBytesWritten(socketTimeoutMs) && _localSocket->waitForReadyRead(socketTimeoutMs);
    };

    const auto mainServerName = getServerNameForPath(filePath);

    if (mainServerName.isEmpty()) {
        return result;
    }

    // #1 Connect to the local server
    if (!connectSocketToServer(mainServerName)) {
        return result;
    }

    auto messageRequestThumbnailForFile = QVariantMap {
        {
            VfsShellExtensions::Protocol::ThumbnailProviderRequestKey,
            QVariantMap {
                {VfsShellExtensions::Protocol::FilePathKey, filePath},
                {VfsShellExtensions::Protocol::ThumbnailProviderRequestFileSizeKey, QVariantMap{{QStringLiteral("width"), size.width()}, {QStringLiteral("height"), size.height()}}}
            }
        }
    };

    // #2 Request a thumbnail of a 'size' for a 'filePath'
    if (!sendMessageAndReadyRead(messageRequestThumbnailForFile)) {
        return result;
    }

    // #3 Read the thumbnail data (read all as the thumbnail size is usually less than 1MB)
    const auto message = QJsonDocument::fromJson(_localSocket->readAll()).toVariant().toMap();
    if (!VfsShellExtensions::Protocol::validateProtocolVersion(message)) {
        return result;
    }
    result = QByteArray::fromBase64(message.value(VfsShellExtensions::Protocol::ThumnailProviderDataKey).toByteArray());
    disconnectSocketFromServer();

    return result;
}

bool ThumbnailProviderIpc::disconnectSocketFromServer()
{
    const auto isConnectedOrConnecting = _localSocket->state() == QLocalSocket::ConnectedState || _localSocket->state() == QLocalSocket::ConnectingState;
    if (isConnectedOrConnecting) {
        _localSocket->disconnectFromServer();
        const auto isNotConnected = _localSocket->state() == QLocalSocket::UnconnectedState || _localSocket->state() == QLocalSocket::ClosingState;
        return isNotConnected || _localSocket->waitForDisconnected();
    }
    return true;
}

QString ThumbnailProviderIpc::getServerNameForPath(const QString &filePath)
{
    if (!overrideServerName.isEmpty()) {
        return overrideServerName;
    }

    return findServerNameForPath(filePath);
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
QString ThumbnailProviderIpc::overrideServerName = {};
}

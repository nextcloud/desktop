/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "customstateprovideripc.h"
#include "common/shellextensionutils.h"
#include "ipccommon.h"
#include <QJsonDocument>
namespace {
// we don't want to block the Explorer for too long (default is 30K, so we'd keep it at 10K, except QLocalSocket::waitForDisconnected())
constexpr auto socketTimeoutMs = 10000;
}

namespace VfsShellExtensions {

CustomStateProviderIpc::~CustomStateProviderIpc()
{
    disconnectSocketFromServer();
}

QVariantList CustomStateProviderIpc::fetchCustomStatesForFile(const QString &filePath)
{
    const auto sendMessageAndReadyRead = [this](QVariantMap &message) {
        _localSocket.write(VfsShellExtensions::Protocol::createJsonMessage(message));
        return _localSocket.waitForBytesWritten(socketTimeoutMs) && _localSocket.waitForReadyRead(socketTimeoutMs);
    };

    const auto mainServerName = getServerNameForPath(filePath);

    if (mainServerName.isEmpty()) {
        return {};
    }

    // #1 Connect to the local server
    if (!connectSocketToServer(mainServerName)) {
        return {};
    }

    auto messageRequestCustomStatesForFile = QVariantMap {
        {
            VfsShellExtensions::Protocol::CustomStateProviderRequestKey,
            QVariantMap {
                { VfsShellExtensions::Protocol::FilePathKey, filePath }
            }
        }
    };

    // #2 Request custom states for a 'filePath'
    if (!sendMessageAndReadyRead(messageRequestCustomStatesForFile)) {
        return {};
    }

    // #3 Receive custom states as JSON
    const auto message = QJsonDocument::fromJson(_localSocket.readAll()).toVariant().toMap();
    if (!VfsShellExtensions::Protocol::validateProtocolVersion(message) || !message.contains(VfsShellExtensions::Protocol::CustomStateDataKey)) {
        return {};
    }
    const auto customStates = message.value(VfsShellExtensions::Protocol::CustomStateDataKey).toMap().value(VfsShellExtensions::Protocol::CustomStateStatesKey).toList();
    disconnectSocketFromServer();

    return customStates;
}

bool CustomStateProviderIpc::disconnectSocketFromServer()
{
    const auto isConnectedOrConnecting = _localSocket.state() == QLocalSocket::ConnectedState || _localSocket.state() == QLocalSocket::ConnectingState;
    if (isConnectedOrConnecting) {
        _localSocket.disconnectFromServer();
        const auto isNotConnected = _localSocket.state() == QLocalSocket::UnconnectedState || _localSocket.state() == QLocalSocket::ClosingState;
        return isNotConnected || _localSocket.waitForDisconnected();
    }
    return true;
}

QString CustomStateProviderIpc::getServerNameForPath(const QString &filePath)
{
    if (!overrideServerName.isEmpty()) {
        return overrideServerName;
    }

    return findServerNameForPath(filePath);
}

bool CustomStateProviderIpc::connectSocketToServer(const QString &serverName)
{
    if (!disconnectSocketFromServer()) {
        return false;
    }
    _localSocket.setServerName(serverName);
    _localSocket.connectToServer();
    return _localSocket.state() == QLocalSocket::ConnectedState || _localSocket.waitForConnected(socketTimeoutMs);
}
QString CustomStateProviderIpc::overrideServerName = {};
}

/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QtNetwork/QLocalSocket>
#include <QString>
#include <QVariant>

namespace VfsShellExtensions {
class CustomStateProviderIpc
{
public:
    CustomStateProviderIpc() = default;
    ~CustomStateProviderIpc();

    QVariantList fetchCustomStatesForFile(const QString &filePath);

private:
    bool connectSocketToServer(const QString &serverName);
    bool disconnectSocketFromServer();

    static QString getServerNameForPath(const QString &filePath);

public:
    // for unit tests (as Registry does not work on a CI VM)
    static QString overrideServerName;

private:
    QLocalSocket _localSocket;
};
}

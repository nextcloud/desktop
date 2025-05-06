/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

class QString;
class QSize;
class QLocalSocket;

#include <QByteArray>
#include <QScopedPointer>

namespace VfsShellExtensions {
class ThumbnailProviderIpc
{
public:
    ThumbnailProviderIpc();
    ~ThumbnailProviderIpc();

    QByteArray fetchThumbnailForFile(const QString &filePath, const QSize &size);

private:
    bool connectSocketToServer(const QString &serverName);
    bool disconnectSocketFromServer();

    static QString getServerNameForPath(const QString &filePath);

public:
    // for unit tests (as Registry does not work on a CI VM)
    static QString overrideServerName;

private:
    QScopedPointer<QLocalSocket> _localSocket;
};
}

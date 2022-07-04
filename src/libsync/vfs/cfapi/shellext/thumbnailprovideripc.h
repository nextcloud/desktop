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

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

#include <QObject>
#include <QLocalServer>
#include <QSize>

class QLocalSocket;

namespace OCC {
class ShellExtensionsServer : public QObject
{
    struct ThumbnailRequestInfo
    {
        QString path;
        QSize size;
        QString folderAlias;

        bool isValid() const { return !path.isEmpty() && !size.isEmpty() && !folderAlias.isEmpty(); }
    };

    Q_OBJECT
public:
    ShellExtensionsServer(QObject *parent = nullptr);
    ~ShellExtensionsServer() override;

private:
    void sendJsonMessageWithVersion(QLocalSocket *socket, const QVariantMap &message);
    void sendEmptyDataAndCloseSession(QLocalSocket *socket);
    void closeSession(QLocalSocket *socket);
    void processThumbnailRequest(QLocalSocket *socket, const ThumbnailRequestInfo &thumbnailRequestInfo);

private slots:
    void slotNewConnection();

private:
    QLocalServer _localServer;
};
} // namespace OCC

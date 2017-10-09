/*
 * Copyright (C) by Dominik Schmidt <dev@dominik-schmidt.de>
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


#ifndef SOCKETAPI_H
#define SOCKETAPI_H

#include "syncfileitem.h"
#include "syncfilestatus.h"
// #include "ownsql.h"

#if defined(Q_OS_MAC)
#include "socketapisocket_mac.h"
#else
#include <QLocalServer>
typedef QLocalServer SocketApiServer;
#endif

class QUrl;
class QLocalSocket;
class QStringList;

namespace OCC {

class SyncFileStatus;
class Folder;
class SocketListener;

/**
 * @brief The SocketApi class
 * @ingroup gui
 */
class SocketApi : public QObject
{
    Q_OBJECT

public:
    explicit SocketApi(QObject *parent = 0);
    virtual ~SocketApi();

public slots:
    void slotUpdateFolderView(Folder *f);
    void slotUnregisterPath(const QString &alias);
    void slotRegisterPath(const QString &alias);
    void broadcastStatusPushMessage(const QString &systemPath, SyncFileStatus fileStatus);

signals:
    void shareCommandReceived(const QString &sharePath, const QString &localPath);

private slots:
    void slotNewConnection();
    void onLostConnection();
    void slotSocketDestroyed(QObject *obj);
    void slotReadSocket();

    void copyPrivateLinkToClipboard(const QString &link) const;
    void emailPrivateLink(const QString &link) const;

private:
    void broadcastMessage(const QString &msg, bool doWait = false);

    Q_INVOKABLE void command_RETRIEVE_FOLDER_STATUS(const QString &argument, SocketListener *listener);
    Q_INVOKABLE void command_RETRIEVE_FILE_STATUS(const QString &argument, SocketListener *listener);

    Q_INVOKABLE void command_VERSION(const QString &argument, SocketListener *listener);

    Q_INVOKABLE void command_SHARE_STATUS(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_SHARE_MENU_TITLE(const QString &argument, SocketListener *listener);

    // The context menu actions
    Q_INVOKABLE void command_SHARE(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_COPY_PRIVATE_LINK(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_EMAIL_PRIVATE_LINK(const QString &localFile, SocketListener *listener);

    /** Sends translated/branded strings that may be useful to the integration */
    Q_INVOKABLE void command_GET_STRINGS(const QString &argument, SocketListener *listener);

    QString buildRegisterPathMessage(const QString &path);

    QSet<QString> _registeredAliases;
    QList<SocketListener> _listeners;
    SocketApiServer _localServer;
};
}
#endif // SOCKETAPI_H

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
#include "common/syncfilestatus.h"
#include "sharedialog.h" // for the ShareDialogStartPage
#include "common/syncjournalfilerecord.h"

#if defined(Q_OS_MAC)
#include "socketapisocket_mac.h"
#else
#include <QLocalServer>
using SocketApiServer = QLocalServer;
using SocketApiSocket = QLocalSocket;
#endif

class QUrl;
class QLocalSocket;

namespace OCC {

class SyncFileStatus;
class Folder;
class SocketListener;
class SocketApiJob;
class SocketApiJobV2;

Q_DECLARE_LOGGING_CATEGORY(lcSocketApi)

/**
 * @brief The SocketApi class
 * @ingroup gui
 */
class SocketApi : public QObject
{
    Q_OBJECT

public:
    explicit SocketApi(QObject *parent = nullptr);
    ~SocketApi() override;

    void startShellIntegration();

public slots:
    void registerAccount(const AccountPtr &a);
    void unregisterAccount(const AccountPtr &a);
    void slotUpdateFolderView(Folder *f);
    void slotUnregisterPath(Folder *f);
    void slotRegisterPath(Folder *f);
    void broadcastStatusPushMessage(const QString &systemPath, SyncFileStatus fileStatus);

signals:
    void shareCommandReceived(const QString &sharePath, const QString &localPath, ShareDialogStartPage startPage);

private slots:
    void slotNewConnection();
    void slotReadSocket();

    static void copyUrlToClipboard(const QUrl &link);
    static void emailPrivateLink(const QUrl &link);
    static void openPrivateLink(const QUrl &link);

private:
    // Helper structure for getting information on a file
    // based on its local path - used for nearly all remote
    // actions.
    struct FileData
    {
        static FileData get(const QString &localFile);
        SyncFileStatus syncFileStatus() const;
        SyncJournalFileRecord journalRecord() const;
        FileData parentFolder() const;

        // Relative path of the file locally, without any vfs suffix
        QString folderRelativePathNoVfsSuffix() const;

        Folder *folder = nullptr;
        // Absolute path of the file locally. (May be a virtual file)
        QString localPath;
        // Relative path of the file locally, as in the DB. (May be a virtual file)
        QString folderRelativePath;
        // Path of the file on the server (In case of virtual file, it points to the actual file)
        QString serverRelativePath;

        bool isSyncFolder() const;

        bool isValid() const;
    };

    void broadcastMessage(const QString &msg, bool doWait = false);

    // opens share dialog, sends reply
    void processShareRequest(const QString &localFile, SocketListener *listener, ShareDialogStartPage startPage);

    Q_INVOKABLE void command_RETRIEVE_FOLDER_STATUS(const QString &argument, SocketListener *listener);
    Q_INVOKABLE void command_RETRIEVE_FILE_STATUS(const QString &argument, SocketListener *listener);

    Q_INVOKABLE void command_VERSION(const QString &argument, SocketListener *listener);

    Q_INVOKABLE void command_SHARE_MENU_TITLE(const QString &argument, SocketListener *listener);

    // The context menu actions
    Q_INVOKABLE void command_SHARE(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_MANAGE_PUBLIC_LINKS(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_COPY_PUBLIC_LINK(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_COPY_PRIVATE_LINK(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_EMAIL_PRIVATE_LINK(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_OPEN_PRIVATE_LINK(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_OPEN_PRIVATE_LINK_VERSIONS(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_MAKE_AVAILABLE_LOCALLY(const QString &filesArg, SocketListener *listener);
    Q_INVOKABLE void command_MAKE_ONLINE_ONLY(const QString &filesArg, SocketListener *listener);
    Q_INVOKABLE void command_DELETE_ITEM(const QString &localFile, SocketListener *listener);
    Q_INVOKABLE void command_MOVE_ITEM(const QString &localFile, SocketListener *listener);

    Q_INVOKABLE void command_OPEN_APP_LINK(const QString &localFile, SocketListener *listener);
    // External sync
    Q_INVOKABLE void command_V2_LIST_ACCOUNTS(const QSharedPointer<SocketApiJobV2> &job) const;

    // Sends the id and the client icon as PNG image (base64 encoded) in Json key "png"
    // e.g. { "id" : "1", "arguments" : { "png" : "hswehs343dj8..." } } or an error message in key "error"
    //
    // Argument is a SocketApiJobV2 job which contains an id and the required icon size in Json format
    // e.g. { "id" : "1", "arguments" : { "size" : 16 } }
    Q_INVOKABLE void command_V2_GET_CLIENT_ICON(const QSharedPointer<SocketApiJobV2> &job) const;

    // Fetch the private link and call targetFun
    void fetchPrivateLinkUrlHelper(const QString &localFile, const std::function<void(const QUrl &url)> &targetFun);

    /** Sends translated/branded strings that may be useful to the integration */
    Q_INVOKABLE void command_GET_STRINGS(const QString &argument, SocketListener *listener);

    // Sends the context menu options relating to sharing to listener
    void sendSharingContextMenuOptions(const FileData &fileData, SocketListener *listener);

    /** Send the list of menu item. (added in version 1.1)
     * argument is a list of files for which the menu should be shown, separated by '\x1e'
     * Reply with  GET_MENU_ITEMS:BEGIN
     * followed by several MENU_ITEM:[Action]:[flag]:[Text]
     * If flag contains 'd', the menu should be disabled
     * and ends with GET_MENU_ITEMS:END
     */
    Q_INVOKABLE void command_GET_MENU_ITEMS(const QString &argument, SocketListener *listener);

    QString buildRegisterPathMessage(const QString &path);

    QString _socketPath;
    QSet<Folder *> _registeredFolders;
    QSet<AccountPtr> _registeredAccounts;
    QMap<SocketApiSocket *, QSharedPointer<SocketListener>> _listeners;
    SocketApiServer _localServer;
};
}

#endif // SOCKETAPI_H

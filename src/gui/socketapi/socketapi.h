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

#include "common/syncfilestatus.h"
#include "common/syncjournalfilerecord.h"
#include "syncfileitem.h"

#include "config.h"

#include <QLocalServer>

class QUrl;
class QLocalSocket;
class QFileInfo;

namespace OCC
{
class SyncFileStatus;
class Folder;
class SocketListener;
class DirectEditor;
class SocketApiJob;
class SocketApiJobV2;

Q_DECLARE_LOGGING_CATEGORY(lcSocketApi)

#ifdef Q_OS_MACOS
QString socketApiSocketPath();
#endif

/**
 * @brief The SocketApi class
 * @ingroup gui
 */
class SocketApi : public QObject
{
    Q_OBJECT

    enum SharingContextItemEncryptedFlag {
        EncryptedItem,
        NotEncryptedItem
    };

    enum SharingContextItemRootEncryptedFolderFlag {
        RootEncryptedFolder,
        NonRootEncryptedFolder
    };

public:
    explicit SocketApi(QObject *parent = nullptr);
    ~SocketApi() override;

public slots:
    void slotUpdateFolderView(OCC::Folder *f);
    void slotUnregisterPath(const QString &alias);
    void slotRegisterPath(const QString &alias);
    void broadcastStatusPushMessage(const QString &systemPath, OCC::SyncFileStatus fileStatus);

signals:
    void shareCommandReceived(const QString &localPath);
    void fileActivityCommandReceived(const QString &localPath);

private slots:
    void slotNewConnection();
    void onLostConnection();
    void slotSocketDestroyed(QObject *obj);
    void slotReadSocket();

    static void copyUrlToClipboard(const QString &link);
    static void emailPrivateLink(const QString &link);
    static void openPrivateLink(const QString &link);

private:
    // Helper structure for getting information on a file
    // based on its local path - used for nearly all remote
    // actions.
    struct FileData
    {
        static FileData get(const QString &localFile);
        [[nodiscard]] SyncFileStatus syncFileStatus() const;
        [[nodiscard]] SyncJournalFileRecord journalRecord() const;
        [[nodiscard]] FileData parentFolder() const;

        // Relative path of the file locally, without any vfs suffix
        [[nodiscard]] QString folderRelativePathNoVfsSuffix() const;

        Folder *folder = nullptr;
        // Absolute path of the file locally. (May be a virtual file)
        QString localPath;
        // Relative path of the file locally, as in the DB. (May be a virtual file)
        QString folderRelativePath;
        // Path of the file on the server (In case of virtual file, it points to the actual file)
        QString serverRelativePath;
    };

    void broadcastMessage(const QString &msg, bool doWait = false);

    // opens share dialog, sends reply
    void processShareRequest(const QString &localFile, SocketListener *listener);
    void processLeaveShareRequest(const QString &localFile, SocketListener *listener);
    void processFileActivityRequest(const QString &localFile);
    void processEncryptRequest(const QString &localFile);

    Q_INVOKABLE void command_RETRIEVE_FOLDER_STATUS(const QString &argument, OCC::SocketListener *listener);
    Q_INVOKABLE void command_RETRIEVE_FILE_STATUS(const QString &argument, OCC::SocketListener *listener);

    Q_INVOKABLE void command_VERSION(const QString &argument, OCC::SocketListener *listener);

    Q_INVOKABLE void command_SHARE_MENU_TITLE(const QString &argument, OCC::SocketListener *listener);

    // The context menu actions
    Q_INVOKABLE void command_ACTIVITY(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_ENCRYPT(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_SHARE(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_LEAVESHARE(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_MANAGE_PUBLIC_LINKS(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_COPY_SECUREFILEDROP_LINK(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_COPY_PUBLIC_LINK(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_COPY_PRIVATE_LINK(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_EMAIL_PRIVATE_LINK(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_OPEN_PRIVATE_LINK(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_MAKE_AVAILABLE_LOCALLY(const QString &filesArg, OCC::SocketListener *listener);
    Q_INVOKABLE void command_MAKE_ONLINE_ONLY(const QString &filesArg, OCC::SocketListener *listener);
    Q_INVOKABLE void command_RESOLVE_CONFLICT(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_DELETE_ITEM(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_MOVE_ITEM(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_LOCK_FILE(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_UNLOCK_FILE(const QString &localFile, OCC::SocketListener *listener);

    void setFileLock(const QString &localFile, const SyncFileItem::LockStatus lockState) const;

    // Windows Shell / Explorer pinning fallbacks, see issue: https://github.com/nextcloud/desktop/issues/1599
#ifdef Q_OS_WIN
    Q_INVOKABLE void command_COPYASPATH(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_OPENNEWWINDOW(const QString &localFile, OCC::SocketListener *listener);
    Q_INVOKABLE void command_OPEN(const QString &localFile, OCC::SocketListener *listener);
#endif

    // External sync
    Q_INVOKABLE void command_V2_LIST_ACCOUNTS(const QSharedPointer<OCC::SocketApiJobV2> &job) const;
    Q_INVOKABLE void command_V2_UPLOAD_FILES_FROM(const QSharedPointer<OCC::SocketApiJobV2> &job) const;

    // Fetch the private link and call targetFun
    void fetchPrivateLinkUrlHelper(const QString &localFile, const std::function<void(const QString &url)> &targetFun);

    /** Sends translated/branded strings that may be useful to the integration */
    Q_INVOKABLE void command_GET_STRINGS(const QString &argument, OCC::SocketListener *listener);

    // Sends the context menu options relating to sharing to listener
    void sendSharingContextMenuOptions(const FileData &fileData, SocketListener *listener, SharingContextItemEncryptedFlag itemEncryptionFlag, SharingContextItemRootEncryptedFolderFlag rootE2eeFolderFlag);

    void sendEncryptFolderCommandMenuEntries(const QFileInfo &fileInfo,
                                             const FileData &fileData,
                                             const bool isE2eEncryptedPath,
                                             const OCC::SocketListener* const listener) const;

    void sendLockFileCommandMenuEntries(const QFileInfo &fileInfo,
                                        Folder *const syncFolder,
                                        const FileData &fileData,
                                        const SocketListener *const listener) const;

    void sendLockFileInfoMenuEntries(const QFileInfo &fileInfo,
                                     Folder* const syncFolder,
                                     const FileData &fileData,
                                     const SocketListener* const listener,
                                     const SyncJournalFileRecord &record) const;

    /** Send the list of menu item. (added in version 1.1)
     * argument is a list of files for which the menu should be shown, separated by '\x1e'
     * Reply with  GET_MENU_ITEMS:BEGIN
     * followed by several MENU_ITEM:[Action]:[flag]:[Text]
     * If flag contains 'd', the menu should be disabled
     * and ends with GET_MENU_ITEMS:END
     */
    Q_INVOKABLE void command_GET_MENU_ITEMS(const QString &argument, OCC::SocketListener *listener);

    /// Direct Editing
    Q_INVOKABLE void command_EDIT(const QString &localFile, OCC::SocketListener *listener);
    DirectEditor* getDirectEditorForLocalFile(const QString &localFile);

#if GUI_TESTING
    Q_INVOKABLE void command_ASYNC_ASSERT_ICON_IS_EQUAL(const QSharedPointer<SocketApiJob> &job);
    Q_INVOKABLE void command_ASYNC_LIST_WIDGETS(const QSharedPointer<SocketApiJob> &job);
    Q_INVOKABLE void command_ASYNC_INVOKE_WIDGET_METHOD(const QSharedPointer<SocketApiJob> &job);
    Q_INVOKABLE void command_ASYNC_GET_WIDGET_PROPERTY(const QSharedPointer<SocketApiJob> &job);
    Q_INVOKABLE void command_ASYNC_SET_WIDGET_PROPERTY(const QSharedPointer<SocketApiJob> &job);
    Q_INVOKABLE void command_ASYNC_WAIT_FOR_WIDGET_SIGNAL(const QSharedPointer<SocketApiJob> &job);
    Q_INVOKABLE void command_ASYNC_TRIGGER_MENU_ACTION(const QSharedPointer<SocketApiJob> &job);
#endif

    QString buildRegisterPathMessage(const QString &path);

    QSet<QString> _registeredAliases;
    QMap<QIODevice *, QSharedPointer<SocketListener>> _listeners;
    QLocalServer _localServer;
};
}

#endif // SOCKETAPI_H

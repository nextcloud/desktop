/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "accountstate.h"
#include "syncfileitem.h"

namespace OCC {

class EditLocallyJob;
using EditLocallyJobPtr = QSharedPointer<EditLocallyJob>;

class Folder;
class SyncResult;

class EditLocallyJob : public QObject
{
    Q_OBJECT

public:
    explicit EditLocallyJob(const AccountStatePtr &accountState,
                            const QString &relPath,
                            QObject *parent = nullptr);

    [[nodiscard]] static OCC::Folder *findFolderForFile(const QString &relPath, const QString &userId);
    [[nodiscard]] static QString prefixSlashToPath(const QString &path);

signals:
    void error(const QString &message, const QString &informativeText);
    void finished();
    void callShowError(const QString &message, const QString &informativeText);

public slots:
    void startSetup();
    void startEditLocally();

private slots:
    void fetchRemoteFileParentInfo();
    void startSyncBeforeOpening();

    void proceedWithSetup();
    void findAfolderAndConstructPaths();

    void showError(const QString &message, const QString &informativeText);

    void slotItemDiscovered(const OCC::SyncFileItemPtr &item);
    void slotItemCompleted(const OCC::SyncFileItemPtr &item);

    void slotLsColJobFinishedWithError(QNetworkReply *reply);
    void slotDirectoryListingIterated(const QString &name, const QMap<QString, QString> &properties);

    void processLocalItem();
    void openFile();
    void lockFile(const QString &etag);

    void fileAlreadyLocked();
    void fileLockSuccess(const OCC::SyncFileItemPtr &item);
    void fileLockError(const QString &errorMessage);
    void fileLockProcedureComplete(const QString &notificationTitle,
                                   const QString &notificationMessage,
                                   const bool success);
    void disconnectFolderSignals();

private:
    [[nodiscard]] bool checkIfFileParentSyncIsNeeded(); // returns true if sync will be needed, false otherwise
    [[nodiscard]] bool eraseBlacklistRecordForItem();
    [[nodiscard]] const QString getRelativePathToRemoteRootForFile() const; // returns either '/' or a (relative path - Folder::remotePath()) for folders pointing to a non-root remote path e.g. '/subfolder' instead of '/'
    [[nodiscard]] const QString getRelativePathParent() const;

    [[nodiscard]] static int fileLockTimeRemainingMinutes(const qint64 lockTime, const qint64 lockTimeOut);

    [[nodiscard]] bool isFileParentItemValid() const;

    bool _shouldScheduleFolderSyncAfterFileIsOpened = false;

    AccountStatePtr _accountState;
    QString _relPath; // full remote path for a file (as on the server)
    QString _relativePathToRemoteRoot; // (relative path - Folder::remotePath()) for folders pointing to a non-root remote path e.g. '/subfolder' instead of '/'
    QString _relPathParent; // a folder where the file resides ('/' if it is in the first level of a remote root, or e.g. a '/subfolder/a/b/c if it resides in a nested folder)
    SyncFileItemPtr _fileParentItem;

    QString _fileName;
    QString _localFilePath;
    QString _folderRelativePath;
    Folder *_folderForFile = nullptr;
    QMetaObject::Connection _syncTerminatedConnection = {};
    QVector<QMetaObject::Connection> _folderConnections;
};

}

/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "owncloudlib.h"

#include "account.h"
#include "rootencryptedfolderinfo.h"
#include "common/syncjournaldb.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSslCertificate>
#include <QString>
#include <QPointer>

namespace OCC {
class FolderMetadata;
class SyncJournalDb;
// all metadata operations with server must be performed via this class
class OWNCLOUDSYNC_EXPORT EncryptedFolderMetadataHandler
    : public QObject
{
    Q_OBJECT

public:
    enum class FetchMode {
        NonEmptyMetadata = 0,
        AllowEmptyMetadata
    };
    Q_ENUM(FetchMode);

    enum class UploadMode {
        DoNotKeepLock = 0,
        KeepLock
    };
    Q_ENUM(UploadMode);

    enum class UnlockFolderWithResult {
        Success = 0,
        Failure
    };
    Q_ENUM(UnlockFolderWithResult);

    explicit EncryptedFolderMetadataHandler(const AccountPtr &account, const QString &folderPath, const QString &remoteFolderRoot, SyncJournalDb *const journalDb, const QString &pathForTopLevelFolder, QObject *parent = nullptr);

    [[nodiscard]] QSharedPointer<FolderMetadata> folderMetadata() const;

    // use this when metadata is already fetched so no fetching will happen in this class
    void setPrefetchedMetadataAndId(const QSharedPointer<FolderMetadata> &metadata, const QByteArray &id);

    // use this when modifying metadata for multiple folders inside top-level one which is locked
    void setFolderToken(const QByteArray &token);
    [[nodiscard]] const QByteArray& folderToken() const;

    [[nodiscard]] const QByteArray& folderId() const;

    [[nodiscard]] bool isUnlockRunning() const;
    [[nodiscard]] bool isFolderLocked() const;

    void fetchMetadata(const RootEncryptedFolderInfo &rootEncryptedFolderInfo, const FetchMode fetchMode = FetchMode::NonEmptyMetadata);
    void fetchMetadata(const FetchMode fetchMode = FetchMode::NonEmptyMetadata);
    void uploadMetadata(const UploadMode uploadMode = UploadMode::DoNotKeepLock);
    void unlockFolder(const UnlockFolderWithResult result = UnlockFolderWithResult::Success);

private:
    void lockFolder();
    void startUploadMetadata();
    void startFetchMetadata();
    void fetchFolderEncryptedId();
    bool validateBeforeLock();

private slots:
    void slotFolderEncryptedIdReceived(const QStringList &list);
    void slotFolderEncryptedIdError(QNetworkReply *reply);

    void slotMetadataReceived(const QJsonDocument &json, int statusCode);
    void slotMetadataReceivedError(const QByteArray &folderId, int httpReturnCode);

    void slotFolderLockedSuccessfully(const QByteArray &folderId, const QByteArray &token);
    void slotFolderLockedError(const QByteArray &folderId, int httpErrorCode);

    void slotUploadMetadataSuccess(const QByteArray &folderId);
    void slotUploadMetadataError(const QByteArray &folderId, int httpReturnCode);

    void slotEmitUploadSuccess();
    void slotEmitUploadError();

public: signals:
    void fetchFinished(int code, const QString &message = {});
    void uploadFinished(int code, const QString &message = {});
    void folderUnlocked(const QByteArray &folderId, int httpStatus);

private:
    AccountPtr _account;
    QPointer<SyncJournalDb> _journalDb;
    QString _folderFullRemotePath;
    QString _remoteFolderRoot;
    QByteArray _folderId;
    QByteArray _folderToken;

    QSharedPointer<FolderMetadata> _folderMetadata;

    RootEncryptedFolderInfo _rootEncryptedFolderInfo;

    int _uploadErrorCode = 200;

    FetchMode _fetchMode = FetchMode::NonEmptyMetadata;
    bool _isFolderLocked = false;
    bool _isUnlockRunning = false;
    bool _isNewMetadataCreated = false;
    UploadMode _uploadMode = UploadMode::DoNotKeepLock;
};

}

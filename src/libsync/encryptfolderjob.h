/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
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

#include "account.h"

namespace OCC {
class SyncJournalDb;

class OWNCLOUDSYNC_EXPORT EncryptFolderJob : public QObject
{
    Q_OBJECT
public:
    enum Status {
        Success = 0,
        Error,
    };
    Q_ENUM(Status)

    explicit EncryptFolderJob(const AccountPtr &account, SyncJournalDb *journal, const QString &path, const QByteArray &fileId, QObject *parent = nullptr);
    void start();

    [[nodiscard]] QString errorString() const;

signals:
    void finished(int status);

private slots:
    void slotEncryptionFlagSuccess(const QByteArray &folderId);
    void slotEncryptionFlagError(const QByteArray &folderId, const int httpReturnCode, const QString &errorMessage);
    void slotLockForEncryptionSuccess(const QByteArray &folderId, const QByteArray &token);
    void slotLockForEncryptionError(const QByteArray &folderId, const int httpReturnCode, const QString &errorMessage);
    void slotUnlockFolderSuccess(const QByteArray &folderId);
    void slotUnlockFolderError(const QByteArray &folderId, const int httpReturnCode, const QString &errorMessage);
    void slotUploadMetadataSuccess(const QByteArray &folderId);
    void slotUpdateMetadataError(const QByteArray &folderId, const int httpReturnCode);

private:
    AccountPtr _account;
    SyncJournalDb *_journal;
    QString _path;
    QByteArray _fileId;
    QByteArray _folderToken;
    QString _errorString;
};
}

/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "account.h"
#include "encryptedfoldermetadatahandler.h"
#include "syncfileitem.h"
#include "owncloudpropagator.h"

#include <QObject>

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

    explicit EncryptFolderJob(const AccountPtr &account,
                              SyncJournalDb *journal,
                              const QString &path,
                              const QString &pathNonEncrypted,
                              const QString &_remoteSyncRootPath,
                              const QByteArray &fileId,
                              OwncloudPropagator *propagator = nullptr,
                              SyncFileItemPtr item = {},
                              QObject *parent = nullptr);
    void start();

    [[nodiscard]] QString errorString() const;

signals:
    void finished(int status, OCC::EncryptionStatusEnums::ItemEncryptionStatus encryptionStatus);

private:
    void uploadMetadata();

private slots:
    void slotEncryptionFlagSuccess(const QByteArray &folderId);
    void slotEncryptionFlagError(const QByteArray &folderId, const int httpReturnCode, const QString &errorMessage);
    void slotUploadMetadataFinished(int statusCode, const QString &message);
    void slotSetEncryptionFlag();

private:
    AccountPtr _account;
    SyncJournalDb *_journal;
    QString _path;
    QString _pathNonEncrypted;
    QString _remoteSyncRootPath;
    QByteArray _fileId;
    QString _errorString;
    OwncloudPropagator *_propagator = nullptr;
    SyncFileItemPtr _item;
    QScopedPointer<EncryptedFolderMetadataHandler> _encryptedFolderMetadataHandler;
};
}

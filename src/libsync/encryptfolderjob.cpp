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

#include "encryptfolderjob.h"

#include "common/syncjournaldb.h"
#include "clientsideencryptionjobs.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcEncryptFolderJob, "nextcloud.sync.propagator.encryptfolder", QtInfoMsg)

EncryptFolderJob::EncryptFolderJob(const AccountPtr &account, SyncJournalDb *journal, const QString &path, const QByteArray &fileId, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _journal(journal)
    , _path(path)
    , _fileId(fileId)
{
}

void EncryptFolderJob::start()
{
    auto job = new OCC::SetEncryptionFlagApiJob(_account, _fileId, OCC::SetEncryptionFlagApiJob::Set, this);
    connect(job, &OCC::SetEncryptionFlagApiJob::success, this, &EncryptFolderJob::slotEncryptionFlagSuccess);
    connect(job, &OCC::SetEncryptionFlagApiJob::error, this, &EncryptFolderJob::slotEncryptionFlagError);
    job->start();
}

QString EncryptFolderJob::errorString() const
{
    return _errorString;
}

void EncryptFolderJob::slotEncryptionFlagSuccess(const QByteArray &fileId)
{
    SyncJournalFileRecord rec;
    if (!_journal->getFileRecord(_path, &rec)) {
        qCWarning(lcEncryptFolderJob) << "could not get file from local DB" << _path;
    }

    if (!rec.isValid()) {
        qCWarning(lcEncryptFolderJob) << "No valid record found in local DB for fileId" << fileId;
    }

    rec._e2eEncryptionStatus = SyncJournalFileRecord::EncryptionStatus::EncryptedMigratedV1_2;
    const auto result = _journal->setFileRecord(rec);
    if (!result) {
        qCWarning(lcEncryptFolderJob) << "Error when setting the file record to the database" << rec._path << result.error();
    }

    const auto lockJob = new LockEncryptFolderApiJob(_account, fileId, _journal, _account->e2e()->_publicKey, this);
    connect(lockJob, &LockEncryptFolderApiJob::success,
            this, &EncryptFolderJob::slotLockForEncryptionSuccess);
    connect(lockJob, &LockEncryptFolderApiJob::error,
            this, &EncryptFolderJob::slotLockForEncryptionError);
    lockJob->start();
}

void EncryptFolderJob::slotEncryptionFlagError(const QByteArray &fileId,
                                               const int httpErrorCode,
                                               const QString &errorMessage)
{
    qDebug() << "Error on the encryption flag of" << fileId << "HTTP code:" << httpErrorCode;
    _errorString = errorMessage;
    emit finished(Error);
}

void EncryptFolderJob::slotLockForEncryptionSuccess(const QByteArray &fileId, const QByteArray &token)
{
    _folderToken = token;

    const FolderMetadata emptyMetadata(_account);
    const auto encryptedMetadata = emptyMetadata.encryptedMetadata();
    if (encryptedMetadata.isEmpty()) {
        //TODO: Mark the folder as unencrypted as the metadata generation failed.
        _errorString = tr("Could not generate the metadata for encryption, Unlocking the folder.\n"
                          "This can be an issue with your OpenSSL libraries.");
        emit finished(Error);
        return;
    }

    auto storeMetadataJob = new StoreMetaDataApiJob(_account, fileId, emptyMetadata.encryptedMetadata(), this);
    connect(storeMetadataJob, &StoreMetaDataApiJob::success,
            this, &EncryptFolderJob::slotUploadMetadataSuccess);
    connect(storeMetadataJob, &StoreMetaDataApiJob::error,
            this, &EncryptFolderJob::slotUpdateMetadataError);
    storeMetadataJob->start();
}

void EncryptFolderJob::slotUploadMetadataSuccess(const QByteArray &folderId)
{
    auto unlockJob = new UnlockEncryptFolderApiJob(_account, folderId, _folderToken, _journal, this);
    connect(unlockJob, &UnlockEncryptFolderApiJob::success,
                    this, &EncryptFolderJob::slotUnlockFolderSuccess);
    connect(unlockJob, &UnlockEncryptFolderApiJob::error,
                    this, &EncryptFolderJob::slotUnlockFolderError);
    unlockJob->start();
}

void EncryptFolderJob::slotUpdateMetadataError(const QByteArray &folderId, const int httpReturnCode)
{
    Q_UNUSED(httpReturnCode);

    const auto unlockJob = new UnlockEncryptFolderApiJob(_account, folderId, _folderToken, _journal, this);
    connect(unlockJob, &UnlockEncryptFolderApiJob::success,
                    this, &EncryptFolderJob::slotUnlockFolderSuccess);
    connect(unlockJob, &UnlockEncryptFolderApiJob::error,
                    this, &EncryptFolderJob::slotUnlockFolderError);
    unlockJob->start();
}

void EncryptFolderJob::slotLockForEncryptionError(const QByteArray &fileId,
                                                  const int httpErrorCode,
                                                  const QString &errorMessage)
{
    qCInfo(lcEncryptFolderJob()) << "Locking error for" << fileId << "HTTP code:" << httpErrorCode;
    _errorString = errorMessage;
    emit finished(Error);
}

void EncryptFolderJob::slotUnlockFolderError(const QByteArray &fileId,
                                             const int httpErrorCode,
                                             const QString &errorMessage)
{
    qCInfo(lcEncryptFolderJob()) << "Unlocking error for" << fileId << "HTTP code:" << httpErrorCode;
    _errorString = errorMessage;
    emit finished(Error);
}
void EncryptFolderJob::slotUnlockFolderSuccess(const QByteArray &fileId)
{
    qCInfo(lcEncryptFolderJob()) << "Unlocking success for" << fileId;
    emit finished(Success);
}

}

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

#include "clientsideencryptionjobs.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcEncryptFolderJob, "nextcloud.sync.propagator.encryptfolder", QtInfoMsg)

EncryptFolderJob::EncryptFolderJob(const AccountPtr &account, const QString &path, const QByteArray &fileId, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _path(path)
    , _fileId(fileId)
{
}

void EncryptFolderJob::start()
{
    auto job = new OCC::SetEncryptionFlagApiJob(_account, _fileId, this);
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
    _account->e2e()->setFolderEncryptedStatus(_path, true);

    auto lockJob = new LockEncryptFolderApiJob(_account, fileId, this);
    connect(lockJob, &LockEncryptFolderApiJob::success,
            this, &EncryptFolderJob::slotLockForEncryptionSuccess);
    connect(lockJob, &LockEncryptFolderApiJob::error,
            this, &EncryptFolderJob::slotLockForEncryptionError);
    lockJob->start();
}

void EncryptFolderJob::slotEncryptionFlagError(const QByteArray &fileId, int httpErrorCode)
{
    qDebug() << "Error on the encryption flag of" << fileId << "HTTP code:" << httpErrorCode;
    emit finished(Error);
}

void EncryptFolderJob::slotLockForEncryptionSuccess(const QByteArray &fileId, const QByteArray &token)
{
    _account->e2e()->setTokenForFolder(fileId, token);

    FolderMetadata emptyMetadata(_account);
    auto encryptedMetadata = emptyMetadata.encryptedMetadata();
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
    const auto token = _account->e2e()->tokenForFolder(folderId);
    auto unlockJob = new UnlockEncryptFolderApiJob(_account, folderId, token, this);
    connect(unlockJob, &UnlockEncryptFolderApiJob::success,
                    this, &EncryptFolderJob::slotUnlockFolderSuccess);
    connect(unlockJob, &UnlockEncryptFolderApiJob::error,
                    this, &EncryptFolderJob::slotUnlockFolderError);
    unlockJob->start();
}

void EncryptFolderJob::slotUpdateMetadataError(const QByteArray &folderId, int httpReturnCode)
{
    Q_UNUSED(httpReturnCode);

    const auto token = _account->e2e()->tokenForFolder(folderId);
    auto unlockJob = new UnlockEncryptFolderApiJob(_account, folderId, token, this);
    connect(unlockJob, &UnlockEncryptFolderApiJob::success,
                    this, &EncryptFolderJob::slotUnlockFolderSuccess);
    connect(unlockJob, &UnlockEncryptFolderApiJob::error,
                    this, &EncryptFolderJob::slotUnlockFolderError);
    unlockJob->start();
}

void EncryptFolderJob::slotLockForEncryptionError(const QByteArray &fileId, int httpErrorCode)
{
    qCInfo(lcEncryptFolderJob()) << "Locking error for" << fileId << "HTTP code:" << httpErrorCode;
    emit finished(Error);
}

void EncryptFolderJob::slotUnlockFolderError(const QByteArray &fileId, int httpErrorCode)
{
    qCInfo(lcEncryptFolderJob()) << "Unlocking error for" << fileId << "HTTP code:" << httpErrorCode;
    emit finished(Error);
}
void EncryptFolderJob::slotUnlockFolderSuccess(const QByteArray &fileId)
{
    qCInfo(lcEncryptFolderJob()) << "Unlocking success for" << fileId;
    emit finished(Success);
}

}

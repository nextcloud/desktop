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

void EncryptFolderJob::slotFetchTopLevelFolderMetadata(const QByteArray &folderId)
{
    const auto job = new GetMetadataApiJob(_account, folderId);
    connect(job, &GetMetadataApiJob::jsonReceived, this, &EncryptFolderJob::slotTopLevelFolderMetadataReceived);
    connect(job, &GetMetadataApiJob::error, this, &EncryptFolderJob::slotTopLevelFolderMetadataError);
    job->start();
}

void EncryptFolderJob::slotFetchTopLevelFolderEncryptedId(const QString &remotePath)
{
    auto job = new LsColJob(_account, remotePath, this);
    job->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
    connect(job, &LsColJob::directoryListingSubfolders, this, &EncryptFolderJob::slotTopLevelFolderEncryptedIdReceived);
    connect(job, &LsColJob::finishedWithError, this, &EncryptFolderJob::slotTopLevelFolderEncryptedIdError);
    job->start();
}

void EncryptFolderJob::slotTopLevelFolderEncryptedIdReceived(const QStringList &list)
{
    const auto job = qobject_cast<LsColJob *>(sender());
    if (!job || list.isEmpty()) {
        emit finished(Error);
        return;
    }
    const auto &folderInfo = job->_folderInfos.value(list.first());
    slotFetchTopLevelFolderMetadata(folderInfo.fileId);
}

void EncryptFolderJob::slotTopLevelFolderEncryptedIdError(QNetworkReply *r)
{
    if (!r) {
        qDebug() << "Error retrieving the Id of the encrypted folder" << _path;
    } else {
        qDebug() << "Error retrieving the Id of the encrypted folder" << _path << "with httpErrorCode"
                                             << r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    emit finished(Error);
}

void EncryptFolderJob::slotTopLevelFolderMetadataReceived(const QJsonDocument &json, int statusCode)
{
    qDebug() << "Metadata received, applying it to the result list";
    _topLevelE2eeFolderMetadata.reset(new FolderMetadata(_account, json.toJson(QJsonDocument::Compact), statusCode));
    slotSetEncryptionFlag();
}

void EncryptFolderJob::slotSetEncryptionFlag()
{
    auto job = new OCC::SetEncryptionFlagApiJob(_account, _fileId, OCC::SetEncryptionFlagApiJob::Set, this);
    connect(job, &OCC::SetEncryptionFlagApiJob::success, this, &EncryptFolderJob::slotEncryptionFlagSuccess);
    connect(job, &OCC::SetEncryptionFlagApiJob::error, this, &EncryptFolderJob::slotEncryptionFlagError);
    job->start();
}

void EncryptFolderJob::slotTopLevelFolderMetadataError(const QByteArray &folderId, int httpReturnCode)
{
    qWarning() << "E2EE Metadata job error. Trying to proceed without it." << folderId << httpReturnCode;
    emit finished(404);
}

void EncryptFolderJob::start()
{
    slotSetEncryptionFlag();
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

    rec._isE2eEncrypted = true;
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

    const auto subPathSplit = _path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const auto topLevelFolderPath = subPathSplit.size() > 1 ? subPathSplit.first() + QStringLiteral("/") : QStringLiteral("/");
    const QSharedPointer<FolderMetadata> emptyMetadata(new FolderMetadata(_account, {}, -1, {}, topLevelFolderPath, _journal));
    connect(emptyMetadata.data(), &FolderMetadata::setupComplete, this, [this, emptyMetadata, fileId] {
        const auto encryptedMetadata = emptyMetadata->encryptedMetadata();
        if (encryptedMetadata.isEmpty()) {
            // TODO: Mark the folder as unencrypted as the metadata generation failed.
            _errorString =
                tr("Could not generate the metadata for encryption, Unlocking the folder.\n"
                   "This can be an issue with your OpenSSL libraries.");
            emit finished(Error);
            return;
        }

        auto storeMetadataJob = new StoreMetaDataApiJob(_account, fileId, emptyMetadata->encryptedMetadata(), this);
        connect(storeMetadataJob, &StoreMetaDataApiJob::success, this, &EncryptFolderJob::slotUploadMetadataSuccess);
        connect(storeMetadataJob, &StoreMetaDataApiJob::error, this, &EncryptFolderJob::slotUpdateMetadataError);
        storeMetadataJob->start();
    });
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

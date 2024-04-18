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
#include "foldermetadata.h"
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcEncryptFolderJob, "nextcloud.sync.propagator.encryptfolder", QtInfoMsg)

EncryptFolderJob::EncryptFolderJob(const AccountPtr &account, SyncJournalDb *journal, const QString &path, const QString &pathNonEncrypted, const QString &remoteSyncRootPath, const QByteArray &fileId, OwncloudPropagator *propagator, SyncFileItemPtr item,
    QObject * parent)
    : QObject(parent)
    , _account(account)
    , _journal(journal)
    , _path(path)
    , _pathNonEncrypted(pathNonEncrypted)
    , _remoteSyncRootPath(remoteSyncRootPath)
    , _fileId(fileId)
    , _propagator(propagator)
    , _item(item)
{
    SyncJournalFileRecord rec;
    const auto currentPath = !_pathNonEncrypted.isEmpty() ? _pathNonEncrypted : _path;
    const auto currentPathRelative = Utility::fullRemotePathToRemoteSyncRootRelative(currentPath, _remoteSyncRootPath);
    const QString fullRemotePath = Utility::trailingSlashPath(Utility::noLeadingSlashPath(_remoteSyncRootPath)) + currentPathRelative;
    [[maybe_unused]] const auto result = _journal->getRootE2eFolderRecord(Utility::fullRemotePathToRemoteSyncRootRelative(currentPath, _remoteSyncRootPath), &rec);
    _encryptedFolderMetadataHandler.reset(new EncryptedFolderMetadataHandler(account, fullRemotePath, _remoteSyncRootPath, _journal, rec.path()));
}

void EncryptFolderJob::slotSetEncryptionFlag()
{
    auto job = new OCC::SetEncryptionFlagApiJob(_account, _fileId, OCC::SetEncryptionFlagApiJob::Set, this);
    connect(job, &OCC::SetEncryptionFlagApiJob::success, this, &EncryptFolderJob::slotEncryptionFlagSuccess);
    connect(job, &OCC::SetEncryptionFlagApiJob::error, this, &EncryptFolderJob::slotEncryptionFlagError);
    job->start();
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
    const auto currentPath = !_pathNonEncrypted.isEmpty() ? _pathNonEncrypted : _path;
    if (!_journal->getFileRecord(currentPath, &rec)) {
        qCWarning(lcEncryptFolderJob) << "could not get file from local DB" << currentPath;
    }

    if (!rec.isValid()) {
        if (_propagator && _item) {
            qCWarning(lcEncryptFolderJob) << "No valid record found in local DB for fileId" << fileId << "going to create it now...";
            const auto updateResult = _propagator->updateMetadata(*_item.data());
            if (updateResult) {
                [[maybe_unused]] const auto result = _journal->getFileRecord(currentPath, &rec);
            }
        } else {
            qCWarning(lcEncryptFolderJob) << "No valid record found in local DB for fileId" << fileId;
        }
    }

    if (!rec.isE2eEncrypted()) {
        rec._e2eEncryptionStatus = SyncJournalFileRecord::EncryptionStatus::Encrypted;
        const auto result = _journal->setFileRecord(rec);
        if (!result) {
            qCWarning(lcEncryptFolderJob) << "Error when setting the file record to the database" << rec._path << result.error();
        }
    }

    uploadMetadata();
}

void EncryptFolderJob::slotEncryptionFlagError(const QByteArray &fileId,
                                               const int httpErrorCode,
                                               const QString &errorMessage)
{
    qDebug() << "Error on the encryption flag of" << fileId << "HTTP code:" << httpErrorCode;
    _errorString = errorMessage;
    emit finished(Error, EncryptionStatusEnums::ItemEncryptionStatus::NotEncrypted);
}

void EncryptFolderJob::uploadMetadata()
{
    const auto currentPath = !_pathNonEncrypted.isEmpty() ? _pathNonEncrypted : _path;
    const auto currentPathRelative = Utility::fullRemotePathToRemoteSyncRootRelative(currentPath, _remoteSyncRootPath);
    SyncJournalFileRecord rec;
    if (!_journal->getRootE2eFolderRecord(currentPathRelative, &rec)) {
        emit finished(Error, EncryptionStatusEnums::ItemEncryptionStatus::NotEncrypted);
        return;
    }

    const auto emptyMetadata(QSharedPointer<FolderMetadata>::create(
        _account,
        _remoteSyncRootPath,
        QByteArray{},
        RootEncryptedFolderInfo(RootEncryptedFolderInfo::createRootPath(currentPathRelative, rec.path())),
        QByteArray{}));

    connect(emptyMetadata.data(), &FolderMetadata::setupComplete, this, [this, emptyMetadata] {
        const auto encryptedMetadata = !emptyMetadata->isValid() ? QByteArray{} : emptyMetadata->encryptedMetadata();
        if (encryptedMetadata.isEmpty()) {
            // TODO: Mark the folder as unencrypted as the metadata generation failed.
            _errorString =
                tr("Could not generate the metadata for encryption, Unlocking the folder.\n"
                   "This can be an issue with your OpenSSL libraries.");
            emit finished(Error, EncryptionStatusEnums::ItemEncryptionStatus::NotEncrypted);
            return;
        }
        _encryptedFolderMetadataHandler->setPrefetchedMetadataAndId(emptyMetadata, _fileId);
        connect(_encryptedFolderMetadataHandler.data(),
                &EncryptedFolderMetadataHandler::uploadFinished,
                this,
                &EncryptFolderJob::slotUploadMetadataFinished);
        _encryptedFolderMetadataHandler->uploadMetadata();
    });
}

void EncryptFolderJob::slotUploadMetadataFinished(int statusCode, const QString &message)
{
    if (statusCode != 200) {
        qCDebug(lcEncryptFolderJob) << "Update metadata error for folder" << _encryptedFolderMetadataHandler->folderId() << "with error"
                                            << message;
        qCDebug(lcEncryptFolderJob()) << "Unlocking the folder.";
        _errorString = message;
        emit finished(Error, EncryptionStatusEnums::ItemEncryptionStatus::NotEncrypted);
        return;
    }
    emit finished(Success, _encryptedFolderMetadataHandler->folderMetadata()->encryptedMetadataEncryptionStatus());
}

}

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

#include "rootencryptedfolderinfo.h"
#include "encryptedfoldermetadatahandler.h"
#include "foldermetadata.h"
#include "account.h"
#include "common/syncjournaldb.h"
#include "clientsideencryptionjobs.h"
#include "clientsideencryption.h"

#include <QLoggingCategory>
#include <QNetworkReply>

namespace OCC {

Q_LOGGING_CATEGORY(lcFetchAndUploadE2eeFolderMetadataJob, "nextcloud.sync.propagator.encryptedfoldermetadatahandler", QtInfoMsg)

}

namespace OCC {

EncryptedFolderMetadataHandler::EncryptedFolderMetadataHandler(const AccountPtr &account,
                                                                         const QString &folderPath,
                                                                         SyncJournalDb *const journalDb,
                                                                         const QString &pathForTopLevelFolder,
                                                                         QObject *parent)
    : QObject(parent)
    , _account(account)
    , _folderPath(folderPath)
    , _journalDb(journalDb)
{
    _rootEncryptedFolderInfo = RootEncryptedFolderInfo(
        RootEncryptedFolderInfo::createRootPath(folderPath, pathForTopLevelFolder));
}

void EncryptedFolderMetadataHandler::fetchMetadata(const FetchMode fetchMode)
{
    _fetchMode = fetchMode;
    fetchFolderEncryptedId();
}

void EncryptedFolderMetadataHandler::fetchMetadata(const RootEncryptedFolderInfo &rootEncryptedFolderInfo, const FetchMode fetchMode)
{
    _rootEncryptedFolderInfo = rootEncryptedFolderInfo;
    if (_rootEncryptedFolderInfo.path.isEmpty()) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error fetching metadata for" << _folderPath << ". Invalid _rootEncryptedFolderInfo!";
        emit fetchFinished(-1, tr("Error fetching metadata."));
        return;
    }
    fetchMetadata(fetchMode);
}

void EncryptedFolderMetadataHandler::uploadMetadata(const UploadMode uploadMode)
{
    _uploadMode = uploadMode;
    if (!_folderToken.isEmpty()) {
        // use existing token
        startUploadMetadata();
        return;
    }
    lockFolder();
}

void EncryptedFolderMetadataHandler::lockFolder()
{
    if (!validateBeforeLock()) {
        return;
    }

    const auto lockJob = new LockEncryptFolderApiJob(_account, _folderId, _journalDb, _account->e2e()->_publicKey, this);
    connect(lockJob, &LockEncryptFolderApiJob::success, this, &EncryptedFolderMetadataHandler::slotFolderLockedSuccessfully);
    connect(lockJob, &LockEncryptFolderApiJob::error, this, &EncryptedFolderMetadataHandler::slotFolderLockedError);
    if (_account->capabilities().clientSideEncryptionVersion() >= 2.0) {
        lockJob->setCounter(folderMetadata()->newCounter());
    }
    lockJob->start();
}

void EncryptedFolderMetadataHandler::startFetchMetadata()
{
    const auto job = new GetMetadataApiJob(_account, _folderId);
    connect(job, &GetMetadataApiJob::jsonReceived, this, &EncryptedFolderMetadataHandler::slotMetadataReceived);
    connect(job, &GetMetadataApiJob::error, this, &EncryptedFolderMetadataHandler::slotMetadataReceivedError);
    job->start();
}

void EncryptedFolderMetadataHandler::fetchFolderEncryptedId()
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Folder is encrypted, let's get the Id from it.";
    const auto job = new LsColJob(_account, _folderPath);
    job->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
    connect(job, &LsColJob::directoryListingSubfolders, this, &EncryptedFolderMetadataHandler::slotFolderEncryptedIdReceived);
    connect(job, &LsColJob::finishedWithError, this, &EncryptedFolderMetadataHandler::slotFolderEncryptedIdError);
    job->start();
}

bool EncryptedFolderMetadataHandler::validateBeforeLock()
{
    //Q_ASSERT(!_isFolderLocked && folderMetadata() && folderMetadata()->isValid() && folderMetadata()->isRootEncryptedFolder());
    if (_isFolderLocked) {
        qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Error locking folder" << _folderId << "already locked";
        emit uploadFinished(-1, tr("Error locking folder."));
        return false;
    }

    if (!folderMetadata() || !folderMetadata()->isValid()) {
        qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Error locking folder" << _folderId << "invalid or null metadata";
        emit uploadFinished(-1, tr("Error locking folder."));
        return false;
    }

    // normally, we should allow locking any nested folder to update its metadata, yet, with the new V2 architecture, this is something we might want to disallow
    /*if (!folderMetadata()->isRootEncryptedFolder()) {
        qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Error locking folder" << _folderId << "as it is not a top level folder";
        emit uploadFinished(-1, tr("Error locking folder."));
        return false;
    }*/
    return true;
}

void EncryptedFolderMetadataHandler::slotFolderEncryptedIdReceived(const QStringList &list)
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Received id of folder. Fetching metadata...";
    const auto job = qobject_cast<LsColJob *>(sender());
    const auto &folderInfo = job->_folderInfos.value(list.first());
    _folderId = folderInfo.fileId;
    startFetchMetadata();
}

void EncryptedFolderMetadataHandler::slotFolderEncryptedIdError(QNetworkReply *reply)
{
    Q_ASSERT(reply);
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Error retrieving the Id of the encrypted folder.";
    if (!reply) {
        emit fetchFinished(-1, tr("Error fetching encrypted folder ID."));
        return;
    }
    const auto errorCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    emit fetchFinished(errorCode, reply->errorString());
}

void EncryptedFolderMetadataHandler::slotMetadataReceived(const QJsonDocument &json, int statusCode)
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Metadata Received, parsing it and decrypting" << json.toVariant();

    const auto job = qobject_cast<GetMetadataApiJob *>(sender());
    Q_ASSERT(job);
    if (!job) {
        qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "slotMetadataReceived must be called from GetMetadataApiJob's signal";
        emit fetchFinished(statusCode, tr("Error fetching metadata."));
        return;
    }

    _fetchMode = FetchMode::NonEmptyMetadata;

    if (statusCode != 200 && statusCode != 404) {
        // neither successfully fetched, nor a folder without a metadata, fail further logic
        qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Error fetching metadata for folder" << _folderPath;
        emit fetchFinished(statusCode, tr("Error fetching metadata."));
        return;
    }

    const auto rawMetadata = statusCode == 404
        ? QByteArray{} : json.toJson(QJsonDocument::Compact);
    const auto metadata(QSharedPointer<FolderMetadata>::create(_account, rawMetadata, _rootEncryptedFolderInfo, job->signature()));
    connect(metadata.data(), &FolderMetadata::setupComplete, this, [this, metadata] {
        if (!metadata->isValid()) {
            qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Error parsing or decrypting metadata for folder" << _folderPath;
            emit fetchFinished(-1, tr("Error parsing or decrypting metadata."));
            return;
        }
        _folderMetadata = metadata;
        emit fetchFinished(200);
    });
}

void EncryptedFolderMetadataHandler::slotMetadataReceivedError(const QByteArray &folderId, int httpReturnCode)
{
    Q_UNUSED(folderId);
    if (_fetchMode == FetchMode::AllowEmptyMetadata) {
        qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Error Getting the encrypted metadata. Pretend we got empty metadata. In case when posting it for the first time.";
        _isNewMetadataCreated = true;
        slotMetadataReceived({}, httpReturnCode);
        return;
    }
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Error Getting the encrypted metadata.";
    emit fetchFinished(httpReturnCode, tr("Error fetching metadata."));
}

void EncryptedFolderMetadataHandler::slotFolderLockedSuccessfully(const QByteArray &folderId, const QByteArray &token)
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Folder" << folderId << "Locked Successfully for Upload, Fetching Metadata";
    _folderToken = token;
    _isFolderLocked = true;
    startUploadMetadata();
}

void EncryptedFolderMetadataHandler::slotFolderLockedError(const QByteArray &folderId, int httpErrorCode)
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Error locking folder" << folderId;
    emit fetchFinished(httpErrorCode, tr("Error locking folder."));
}

void EncryptedFolderMetadataHandler::unlockFolder(const UnlockFolderWithResult result)
{
    Q_ASSERT(!_isUnlockRunning);
    Q_ASSERT(_isFolderLocked);

    if (_isUnlockRunning) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Double-call to unlockFolder.";
        return;
    }

    if (!_isFolderLocked) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Folder is not locked.";
        emit folderUnlocked(_folderId, 204);
        return;
    }

    if (_uploadMode == UploadMode::DoNotKeepLock) {
        if (result == UnlockFolderWithResult::Success) {
            connect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadSuccess);
        } else {
            connect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadError);
        }
    }

    if (_folderToken.isEmpty()) {
        emit folderUnlocked(_folderId, 200);
        return;
    }

    _isUnlockRunning = true;

    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Calling Unlock";

    const auto unlockJob = new UnlockEncryptFolderApiJob(_account, _folderId, _folderToken, _journalDb, this);
    connect(unlockJob, &UnlockEncryptFolderApiJob::success, [this](const QByteArray &folderId) {
        qDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Successfully Unlocked";
        _isFolderLocked = false;
        emit folderUnlocked(folderId, 200);
        _isUnlockRunning = false;
    });
    connect(unlockJob, &UnlockEncryptFolderApiJob::error, [this](const QByteArray &folderId, int httpStatus) {
        qDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Unlock Error";
        emit folderUnlocked(folderId, httpStatus);
        _isUnlockRunning = false;
    });
    unlockJob->start();
}

void EncryptedFolderMetadataHandler::startUploadMetadata()
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Metadata created, sending to the server.";

    _uploadErrorCode = 200;

    if (!folderMetadata() || !folderMetadata()->isValid()) {
        slotUploadMetadataError(_folderId, -1);
        return;
    }

    const auto encryptedMetadata = folderMetadata()->encryptedMetadata();
    if (_isNewMetadataCreated) {
        const auto job = new StoreMetaDataApiJob(_account, _folderId, _folderToken, encryptedMetadata, folderMetadata()->metadataSignature());
        connect(job, &StoreMetaDataApiJob::success, this, &EncryptedFolderMetadataHandler::slotUploadMetadataSuccess);
        connect(job, &StoreMetaDataApiJob::error, this, &EncryptedFolderMetadataHandler::slotUploadMetadataError);
        job->start();
    } else {
        const auto job = new UpdateMetadataApiJob(_account, _folderId, encryptedMetadata, _folderToken, folderMetadata()->metadataSignature());
        connect(job, &UpdateMetadataApiJob::success, this, &EncryptedFolderMetadataHandler::slotUploadMetadataSuccess);
        connect(job, &UpdateMetadataApiJob::error, this, &EncryptedFolderMetadataHandler::slotUploadMetadataError);
        job->start();
    }
}

void EncryptedFolderMetadataHandler::slotUploadMetadataSuccess(const QByteArray &folderId)
{
    Q_UNUSED(folderId);
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Uploading of the metadata success.";
    if (_uploadMode == UploadMode::KeepLock || !_isFolderLocked) {
        slotEmitUploadSuccess();
        return;
    }
    connect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadSuccess);
    unlockFolder(UnlockFolderWithResult::Success);
}

void EncryptedFolderMetadataHandler::slotUploadMetadataError(const QByteArray &folderId, int httpReturnCode)
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Update metadata error for folder" << folderId << "with error" << httpReturnCode;
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Unlocking the folder.";
    _uploadErrorCode = httpReturnCode;
    if (_isFolderLocked && _uploadMode == UploadMode::DoNotKeepLock) {
        connect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadError);
        unlockFolder(UnlockFolderWithResult::Failure);
        return;
    }
    emit uploadFinished(_uploadErrorCode);
}

void EncryptedFolderMetadataHandler::slotEmitUploadSuccess()
{
    disconnect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadSuccess);
    emit uploadFinished(_uploadErrorCode);
}

void EncryptedFolderMetadataHandler::slotEmitUploadError()
{
    disconnect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadError);
    emit uploadFinished(_uploadErrorCode, tr("Failed to upload metadata"));
}

QSharedPointer<FolderMetadata> EncryptedFolderMetadataHandler::folderMetadata() const
{
    return _folderMetadata;
}

void EncryptedFolderMetadataHandler::setPrefetchedMetadataAndId(const QSharedPointer<FolderMetadata> &metadata, const QByteArray &id)
{
    Q_ASSERT(metadata && metadata->isValid());
    Q_ASSERT(!id.isEmpty());

    if (!metadata || !metadata->isValid()) {
        qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "invalid metadata argument";
        return;
    }

    if (id.isEmpty()) {
        qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "invalid id argument";
        return;
    }

    _folderId = id;
    _folderMetadata = metadata;
    _isNewMetadataCreated = metadata->initialMetadata().isEmpty();
}

const QByteArray& EncryptedFolderMetadataHandler::folderId() const
{
    return _folderId;
}

void EncryptedFolderMetadataHandler::setFolderToken(const QByteArray &token)
{
    _folderToken = token;
}

const QByteArray& EncryptedFolderMetadataHandler::folderToken() const
{
    return _folderToken;
}

bool EncryptedFolderMetadataHandler::isUnlockRunning() const
{
    return _isUnlockRunning;
}

bool EncryptedFolderMetadataHandler::isFolderLocked() const
{
    return _isFolderLocked;
}

}

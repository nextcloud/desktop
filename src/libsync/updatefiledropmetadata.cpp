/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "updatefiledropmetadata.h"

#include "account.h"
#include "clientsideencryptionjobs.h"
#include "clientsideencryption.h"
#include "syncfileitem.h"

#include <QLoggingCategory>
#include <QNetworkReply>

namespace OCC {

Q_LOGGING_CATEGORY(lcUpdateFileDropMetadataJob, "nextcloud.sync.propagator.updatefiledropmetadatajob", QtInfoMsg)

}

namespace OCC {

UpdateFileDropMetadataJob::UpdateFileDropMetadataJob(OwncloudPropagator *propagator, const QString &path)
    : PropagatorJob(propagator)
    , _path(path)
{
}

void UpdateFileDropMetadataJob::start()
{
    qCDebug(lcUpdateFileDropMetadataJob) << "Folder is encrypted, let's get the Id from it.";
    const auto fetchFolderEncryptedIdJob = new LsColJob(propagator()->account(), _path, this);
    fetchFolderEncryptedIdJob->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
    connect(fetchFolderEncryptedIdJob, &LsColJob::directoryListingSubfolders, this, &UpdateFileDropMetadataJob::slotFolderEncryptedIdReceived);
    connect(fetchFolderEncryptedIdJob, &LsColJob::finishedWithError, this, &UpdateFileDropMetadataJob::slotFolderEncryptedIdError);
    fetchFolderEncryptedIdJob->start();
}

bool UpdateFileDropMetadataJob::scheduleSelfOrChild()
{
    if (_state == Finished) {
        return false;
    }

    if (_state == NotYetStarted) {
        _state = Running;
        start();
    }

    return true;
}

PropagatorJob::JobParallelism UpdateFileDropMetadataJob::parallelism() const
{
    return PropagatorJob::JobParallelism::WaitForFinished;
}

void UpdateFileDropMetadataJob::slotFolderEncryptedIdReceived(const QStringList &list)
{
    qCDebug(lcUpdateFileDropMetadataJob) << "Received id of folder, trying to lock it so we can prepare the metadata";
    const auto fetchFolderEncryptedIdJob = qobject_cast<LsColJob *>(sender());
    Q_ASSERT(fetchFolderEncryptedIdJob);
    if (!fetchFolderEncryptedIdJob) {
        qCCritical(lcUpdateFileDropMetadataJob) << "slotFolderEncryptedIdReceived must be called by a signal";
        emit finished(SyncFileItem::Status::FatalError);
        return;
    }
    Q_ASSERT(!list.isEmpty());
    if (list.isEmpty()) {
        qCCritical(lcUpdateFileDropMetadataJob) << "slotFolderEncryptedIdReceived list.isEmpty()";
        emit finished(SyncFileItem::Status::FatalError);
        return;
    }
    const auto &folderInfo = fetchFolderEncryptedIdJob->_folderInfos.value(list.first());
    slotTryLock(folderInfo.fileId);
}

void UpdateFileDropMetadataJob::slotTryLock(const QByteArray &fileId)
{
    const auto lockJob = new LockEncryptFolderApiJob(propagator()->account(), fileId, propagator()->_journal, propagator()->account()->e2e()->_publicKey, this);
    connect(lockJob, &LockEncryptFolderApiJob::success, this, &UpdateFileDropMetadataJob::slotFolderLockedSuccessfully);
    connect(lockJob, &LockEncryptFolderApiJob::error, this, &UpdateFileDropMetadataJob::slotFolderLockedError);
    lockJob->start();
}

void UpdateFileDropMetadataJob::slotFolderLockedSuccessfully(const QByteArray &fileId, const QByteArray &token)
{
    qCDebug(lcUpdateFileDropMetadataJob) << "Folder" << fileId << "Locked Successfully for Upload, Fetching Metadata"; 
    _folderToken = token;
    _folderId = fileId;
    _isFolderLocked = true;

    const auto fetchMetadataJob = new GetMetadataApiJob(propagator()->account(), _folderId);
    connect(fetchMetadataJob, &GetMetadataApiJob::jsonReceived, this, &UpdateFileDropMetadataJob::slotFolderEncryptedMetadataReceived);
    connect(fetchMetadataJob, &GetMetadataApiJob::error, this, &UpdateFileDropMetadataJob::slotFolderEncryptedMetadataError);

    fetchMetadataJob->start();
}

void UpdateFileDropMetadataJob::slotFolderEncryptedMetadataError(const QByteArray &fileId, int httpReturnCode)
{
    Q_UNUSED(fileId);
    Q_UNUSED(httpReturnCode);
    qCDebug(lcUpdateFileDropMetadataJob()) << "Error Getting the encrypted metadata. Pretend we got empty metadata.";
    const FolderMetadata emptyMetadata(propagator()->account());
    const auto encryptedMetadataJson = QJsonDocument::fromJson(emptyMetadata.encryptedMetadata());
    slotFolderEncryptedMetadataReceived(encryptedMetadataJson, httpReturnCode);
}

void UpdateFileDropMetadataJob::slotFolderEncryptedMetadataReceived(const QJsonDocument &json, int statusCode)
{
    qCDebug(lcUpdateFileDropMetadataJob) << "Metadata Received, Preparing it for the new file." << json.toVariant();

    // Encrypt File!
    _metadata.reset(new FolderMetadata(propagator()->account(),
                                       FolderMetadata::RequiredMetadataVersion::Version1,
                                       json.toJson(QJsonDocument::Compact), statusCode));
    if (!_metadata->moveFromFileDropToFiles() && !_metadata->encryptedMetadataNeedUpdate()) {
        unlockFolder();
        return;
    }

    emit fileDropMetadataParsedAndAdjusted(_metadata.data());

    const auto updateMetadataJob = new UpdateMetadataApiJob(propagator()->account(), _folderId, _metadata->encryptedMetadata(), _folderToken);
    connect(updateMetadataJob, &UpdateMetadataApiJob::success, this, &UpdateFileDropMetadataJob::slotUpdateMetadataSuccess);
    connect(updateMetadataJob, &UpdateMetadataApiJob::error, this, &UpdateFileDropMetadataJob::slotUpdateMetadataError);
    updateMetadataJob->start();
}

void UpdateFileDropMetadataJob::slotUpdateMetadataSuccess(const QByteArray &fileId)
{
    Q_UNUSED(fileId);
    qCDebug(lcUpdateFileDropMetadataJob) << "Uploading of the metadata success, Encrypting the file";

    qCDebug(lcUpdateFileDropMetadataJob) << "Finalizing the upload part, now the actual uploader will take over";
    unlockFolder();
}

void UpdateFileDropMetadataJob::slotUpdateMetadataError(const QByteArray &fileId, int httpErrorResponse)
{
    qCDebug(lcUpdateFileDropMetadataJob) << "Update metadata error for folder" << fileId << "with error" << httpErrorResponse;
    qCDebug(lcUpdateFileDropMetadataJob()) << "Unlocking the folder.";
    unlockFolder();
}

void UpdateFileDropMetadataJob::slotFolderLockedError(const QByteArray &fileId, int httpErrorCode)
{
    Q_UNUSED(httpErrorCode);
    qCDebug(lcUpdateFileDropMetadataJob) << "Folder" << fileId << "with path" << _path << "Coundn't be locked. httpErrorCode" << httpErrorCode;
    emit finished(SyncFileItem::Status::NormalError);
}

void UpdateFileDropMetadataJob::slotFolderEncryptedIdError(QNetworkReply *reply)
{
    if (!reply) {
        qCDebug(lcUpdateFileDropMetadataJob) << "Error retrieving the Id of the encrypted folder" << _path;
    } else {
        qCDebug(lcUpdateFileDropMetadataJob) << "Error retrieving the Id of the encrypted folder" << _path << "with httpErrorCode" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    emit finished(SyncFileItem::Status::NormalError);
}

void UpdateFileDropMetadataJob::unlockFolder()
{
    Q_ASSERT(!_isUnlockRunning);

    if (!_isFolderLocked) {
        emit finished(SyncFileItem::Status::Success);
        return;
    }

    if (_isUnlockRunning) {
        qCWarning(lcUpdateFileDropMetadataJob) << "Double-call to unlockFolder.";
        return;
    }

    _isUnlockRunning = true;

    qCDebug(lcUpdateFileDropMetadataJob) << "Calling Unlock";
    const auto unlockJob = new UnlockEncryptFolderApiJob(propagator()->account(), _folderId, _folderToken, propagator()->_journal, this);

    connect(unlockJob, &UnlockEncryptFolderApiJob::success, [this](const QByteArray &folderId) {
        qCDebug(lcUpdateFileDropMetadataJob) << "Successfully Unlocked";
        _folderToken = "";
        _folderId = "";
        _isFolderLocked = false;

        emit folderUnlocked(folderId, 200);
        _isUnlockRunning = false;
        emit finished(SyncFileItem::Status::Success);
    });
    connect(unlockJob, &UnlockEncryptFolderApiJob::error, [this](const QByteArray &folderId, int httpStatus) {
        qCDebug(lcUpdateFileDropMetadataJob) << "Unlock Error";

        emit folderUnlocked(folderId, httpStatus);
        _isUnlockRunning = false;
        emit finished(SyncFileItem::Status::NormalError);
    });
    unlockJob->start();
}


}

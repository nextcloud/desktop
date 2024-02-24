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

#include <QFileInfo>
#include <QLoggingCategory>
#include "foldermetadata.h"
#include "basepropagateremotedeleteencrypted.h"
#include "account.h"
#include "clientsideencryptionjobs.h"
#include "deletejob.h"
#include "owncloudpropagator.h"

Q_LOGGING_CATEGORY(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED, "nextcloud.sync.propagator.remove.encrypted")

namespace OCC {

BasePropagateRemoteDeleteEncrypted::BasePropagateRemoteDeleteEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item, QObject *parent)
    : QObject(parent)
    , _propagator(propagator)
    , _item(item)
{}

QNetworkReply::NetworkError BasePropagateRemoteDeleteEncrypted::networkError() const
{
    return _networkError;
}

QString BasePropagateRemoteDeleteEncrypted::errorString() const
{
    return _errorString;
}

void BasePropagateRemoteDeleteEncrypted::storeFirstError(QNetworkReply::NetworkError err)
{
    if (_networkError == QNetworkReply::NetworkError::NoError) {
        _networkError = err;
    }
}

void BasePropagateRemoteDeleteEncrypted::storeFirstErrorString(const QString &errString)
{
    if (_errorString.isEmpty()) {
        _errorString = errString;
    }
}

void BasePropagateRemoteDeleteEncrypted::fetchMetadataForPath(const QString &path)
{
    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Folder is encrypted, let's its metadata.";
    _fullFolderRemotePath = _propagator->fullRemotePath(path);

    SyncJournalFileRecord rec;
    if (!_propagator->_journal->getRootE2eFolderRecord(Utility::fullRemotePathToRemoteSyncRootRelative(_fullFolderRemotePath, _propagator->remotePath()), &rec) || !rec.isValid()) {
        taskFailed();
        return;
    }

    _encryptedFolderMetadataHandler.reset(new EncryptedFolderMetadataHandler(_propagator->account(),
                                                                                       _fullFolderRemotePath,
                                                                                       _propagator->_journal,
                                                                                       rec.path()));

    connect(_encryptedFolderMetadataHandler.data(),
            &EncryptedFolderMetadataHandler::fetchFinished,
            this,
            &BasePropagateRemoteDeleteEncrypted::slotFetchMetadataJobFinished);
    connect(_encryptedFolderMetadataHandler.data(),
            &EncryptedFolderMetadataHandler::uploadFinished,
            this,
            &BasePropagateRemoteDeleteEncrypted::slotUpdateMetadataJobFinished);
    _encryptedFolderMetadataHandler->fetchMetadata();
}

void BasePropagateRemoteDeleteEncrypted::uploadMetadata(const EncryptedFolderMetadataHandler::UploadMode uploadMode)
{
    _encryptedFolderMetadataHandler->uploadMetadata(uploadMode);
}

void BasePropagateRemoteDeleteEncrypted::slotFolderUnLockFinished(const QByteArray &folderId, int statusCode)
{
    if (statusCode != 200) {
        _item->_httpErrorCode = statusCode;
        _errorString = tr("\"%1 Failed to unlock encrypted folder %2\".").arg(statusCode).arg(QString::fromUtf8(folderId));
        _item->_errorString = _errorString;
        taskFailed();
        return;
    }
    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Folder id" << folderId << "successfully unlocked";
}

void BasePropagateRemoteDeleteEncrypted::slotDeleteRemoteItemFinished()
{
    auto *deleteJob = qobject_cast<DeleteJob *>(QObject::sender());

    Q_ASSERT(deleteJob);

    if (!deleteJob) {
        qCCritical(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Sender is not a DeleteJob instance.";
        taskFailed();
        return;
    }

    const auto err = deleteJob->reply()->error();

    _item->_httpErrorCode = deleteJob->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = deleteJob->responseTimestamp();
    _item->_requestId = deleteJob->requestId();

    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {
        storeFirstErrorString(deleteJob->errorString());
        storeFirstError(err);

        taskFailed();
        return;
    }

    // A 404 reply is also considered a success here: We want to make sure
    // a file is gone from the server. It not being there in the first place
    // is ok. This will happen for files that are in the DB but not on
    // the server or the local file system.
    if (_item->_httpErrorCode != 204 && _item->_httpErrorCode != 404) {
        // Normally we expect "204 No Content"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        storeFirstErrorString(tr("Wrong HTTP code returned by server. Expected 204, but received \"%1 %2\".")
                       .arg(_item->_httpErrorCode)
                       .arg(deleteJob->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));

        taskFailed();
        return;
    }

    if (!_propagator->_journal->deleteFileRecord(_item->_originalFile, _item->isDirectory())) {
        qCWarning(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Failed to delete file record from local DB" << _item->_originalFile;
    }
    _propagator->_journal->commit("Remote Remove");

    unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Success);
}

void BasePropagateRemoteDeleteEncrypted::deleteRemoteItem(const QString &filename)
{
    qCInfo(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Deleting nested encrypted item" << filename;

    const auto deleteJob = new DeleteJob(_propagator->account(), _propagator->fullRemotePath(filename), this);
    if (_encryptedFolderMetadataHandler && _encryptedFolderMetadataHandler->folderMetadata()
        && _encryptedFolderMetadataHandler->folderMetadata()->isValid()) {
        deleteJob->setFolderToken(_encryptedFolderMetadataHandler->folderToken());
    }

    connect(deleteJob, &DeleteJob::finishedSignal, this, &BasePropagateRemoteDeleteEncrypted::slotDeleteRemoteItemFinished);
    deleteJob->start();
}

void BasePropagateRemoteDeleteEncrypted::unlockFolder(const EncryptedFolderMetadataHandler::UnlockFolderWithResult result)
{
    if (!_encryptedFolderMetadataHandler) {
        qCWarning(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Null _encryptedFolderMetadataHandler";
    }
    if (!_encryptedFolderMetadataHandler || !_encryptedFolderMetadataHandler->isFolderLocked()) {
        emit finished(true);
        return;
    }

    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Unlocking folder" << _encryptedFolderMetadataHandler->folderId();
    
    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::folderUnlocked, this, &BasePropagateRemoteDeleteEncrypted::slotFolderUnLockFinished);
    _encryptedFolderMetadataHandler->unlockFolder(result);
}

void BasePropagateRemoteDeleteEncrypted::taskFailed()
{
    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Task failed for job" << sender();
    _isTaskFailed = true;
    if (_encryptedFolderMetadataHandler && _encryptedFolderMetadataHandler->isFolderLocked()) {
        unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Failure);
    } else {
        emit finished(false);
    }
}

QSharedPointer<FolderMetadata> BasePropagateRemoteDeleteEncrypted::folderMetadata() const
{
    Q_ASSERT(_encryptedFolderMetadataHandler->folderMetadata());
    if (!_encryptedFolderMetadataHandler->folderMetadata()) {
        qCWarning(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Metadata is null!";
    }
    return _encryptedFolderMetadataHandler->folderMetadata();
}

const QByteArray BasePropagateRemoteDeleteEncrypted::folderToken() const
{
    return _encryptedFolderMetadataHandler->folderToken();
}

} // namespace OCC

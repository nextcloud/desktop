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

#include "abstractpropagateremotedeleteencrypted.h"
#include "account.h"
#include "clientsideencryptionjobs.h"
#include "deletejob.h"
#include "owncloudpropagator.h"

Q_LOGGING_CATEGORY(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED, "nextcloud.sync.propagator.remove.encrypted")

namespace OCC {

AbstractPropagateRemoteDeleteEncrypted::AbstractPropagateRemoteDeleteEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item, QObject *parent)
    : QObject(parent)
    , _propagator(propagator)
    , _item(item)
{}

QNetworkReply::NetworkError AbstractPropagateRemoteDeleteEncrypted::networkError() const
{
    return _networkError;
}

QString AbstractPropagateRemoteDeleteEncrypted::errorString() const
{
    return _errorString;
}

void AbstractPropagateRemoteDeleteEncrypted::storeFirstError(QNetworkReply::NetworkError err)
{
    if (_networkError == QNetworkReply::NetworkError::NoError) {
        _networkError = err;
    }
}

void AbstractPropagateRemoteDeleteEncrypted::storeFirstErrorString(const QString &errString)
{
    if (_errorString.isEmpty()) {
        _errorString = errString;
    }
}

void AbstractPropagateRemoteDeleteEncrypted::startLsColJob(const QString &path)
{
    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Folder is encrypted, let's get the Id from it.";
    auto job = new LsColJob(_propagator->account(), _propagator->fullRemotePath(path), this);
    job->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
    connect(job, &LsColJob::directoryListingSubfolders, this, &AbstractPropagateRemoteDeleteEncrypted::slotFolderEncryptedIdReceived);
    connect(job, &LsColJob::finishedWithError, this, &AbstractPropagateRemoteDeleteEncrypted::taskFailed);
    job->start();
}

void AbstractPropagateRemoteDeleteEncrypted::slotFolderEncryptedIdReceived(const QStringList &list)
{
    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Received id of folder, trying to lock it so we can prepare the metadata";
    auto job = qobject_cast<LsColJob *>(sender());
    const ExtraFolderInfo folderInfo = job->_folderInfos.value(list.first());
    slotTryLock(folderInfo.fileId);
}

void AbstractPropagateRemoteDeleteEncrypted::slotTryLock(const QByteArray &folderId)
{
    auto lockJob = new LockEncryptFolderApiJob(_propagator->account(), folderId, this);
    connect(lockJob, &LockEncryptFolderApiJob::success, this, &AbstractPropagateRemoteDeleteEncrypted::slotFolderLockedSuccessfully);
    connect(lockJob, &LockEncryptFolderApiJob::error, this, &AbstractPropagateRemoteDeleteEncrypted::taskFailed);
    lockJob->start();
}

void AbstractPropagateRemoteDeleteEncrypted::slotFolderLockedSuccessfully(const QByteArray &folderId, const QByteArray &token)
{
    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Folder id" << folderId << "Locked Successfully for Upload, Fetching Metadata";
    _folderLocked = true;
    _folderToken = token;
    _folderId = folderId;

    auto job = new GetMetadataApiJob(_propagator->account(), _folderId);
    connect(job, &GetMetadataApiJob::jsonReceived, this, &AbstractPropagateRemoteDeleteEncrypted::slotFolderEncryptedMetadataReceived);
    connect(job, &GetMetadataApiJob::error, this, &AbstractPropagateRemoteDeleteEncrypted::taskFailed);
    job->start();
}

void AbstractPropagateRemoteDeleteEncrypted::slotFolderUnLockedSuccessfully(const QByteArray &folderId)
{
    Q_UNUSED(folderId);
    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Folder id" << folderId << "successfully unlocked";
    _folderLocked = false;
    _folderToken = "";
}

void AbstractPropagateRemoteDeleteEncrypted::slotDeleteRemoteItemFinished()
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

    _propagator->_journal->deleteFileRecord(_item->_originalFile, _item->isDirectory());
    _propagator->_journal->commit("Remote Remove");

    unlockFolder();
}

void AbstractPropagateRemoteDeleteEncrypted::deleteRemoteItem(const QString &filename)
{
    qCInfo(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Deleting nested encrypted item" << filename;

    auto deleteJob = new DeleteJob(_propagator->account(), _propagator->fullRemotePath(filename), this);
    deleteJob->setFolderToken(_folderToken);

    connect(deleteJob, &DeleteJob::finishedSignal, this, &AbstractPropagateRemoteDeleteEncrypted::slotDeleteRemoteItemFinished);

    deleteJob->start();
}

void AbstractPropagateRemoteDeleteEncrypted::unlockFolder()
{
    if (!_folderLocked) {
        emit finished(true);
        return;
    }

    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Unlocking folder" << _folderId;
    auto unlockJob = new UnlockEncryptFolderApiJob(_propagator->account(), _folderId, _folderToken, this);

    connect(unlockJob, &UnlockEncryptFolderApiJob::success, this, &AbstractPropagateRemoteDeleteEncrypted::slotFolderUnLockedSuccessfully);
    connect(unlockJob, &UnlockEncryptFolderApiJob::error, this, [this] (const QByteArray& fileId, int httpReturnCode) {
        Q_UNUSED(fileId);
        _folderLocked = false;
        _folderToken = "";
        _item->_httpErrorCode = httpReturnCode;
        _errorString = tr("\"%1 Failed to unlock encrypted folder %2\".")
                .arg(httpReturnCode)
                .arg(QString::fromUtf8(fileId));
        _item->_errorString =_errorString;
        taskFailed();
    });
    unlockJob->start();
}

void AbstractPropagateRemoteDeleteEncrypted::taskFailed()
{
    qCDebug(ABSTRACT_PROPAGATE_REMOVE_ENCRYPTED) << "Task failed for job" << sender();
    _isTaskFailed = true;
    if (_folderLocked) {
        unlockFolder();
    } else {
        emit finished(false);
    }
}

} // namespace OCC

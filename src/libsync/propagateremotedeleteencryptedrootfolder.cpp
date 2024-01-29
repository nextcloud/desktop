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

/*
 * Removing the root encrypted folder is consisted of multiple steps:
 * - 1st step is to obtain the folderID via LsColJob so it then can be used for the next step
 * - 2nd step is to lock the root folder using the folderID from the previous step. !!! NOTE: If there are no nested items in the folder, this, and subsequent steps are skipped until step 7.
 * - 3rd step is to obtain the root folder's metadata (it contains list of nested files and folders)
 * - 4th step is to remove the nested files and folders from the metadata and send it to the server via UpdateMetadataApiJob
 * - 5th step is to trigger DeleteJob for every nested file and folder of the root folder
 * - 6th step is to unlock the root folder using the previously obtained token from locking
 * - 7th step is to decrypt and delete the root folder, because it is now possible as it has become empty
 */

#include <QFileInfo>
#include <QLoggingCategory>

#include "deletejob.h"
#include "clientsideencryptionjobs.h"
#include "clientsideencryption.h"
#include "foldermetadata.h"
#include "encryptfolderjob.h"
#include "owncloudpropagator.h"
#include "propagateremotedeleteencryptedrootfolder.h"

namespace {
  const char* encryptedFileNamePropertyKey = "encryptedFileName";
}

using namespace OCC;

Q_LOGGING_CATEGORY(PROPAGATE_REMOVE_ENCRYPTED_ROOTFOLDER, "nextcloud.sync.propagator.remove.encrypted.rootfolder")

PropagateRemoteDeleteEncryptedRootFolder::PropagateRemoteDeleteEncryptedRootFolder(OwncloudPropagator *propagator, SyncFileItemPtr item, QObject *parent)
    : BasePropagateRemoteDeleteEncrypted(propagator, item, parent)
{

}

void PropagateRemoteDeleteEncryptedRootFolder::start()
{
    Q_ASSERT(_item->isEncrypted());

    const bool listFilesResult = _propagator->_journal->listFilesInPath(_item->_file.toUtf8(), [this](const OCC::SyncJournalFileRecord &record) {
        _nestedItems[record._e2eMangledName] = record;
    });

    if (!listFilesResult || _nestedItems.isEmpty()) {
        // if the folder is empty, just decrypt and delete it
        decryptAndRemoteDelete();
        return;
    }

    fetchMetadataForPath(_item->_file);
}

void PropagateRemoteDeleteEncryptedRootFolder::slotFolderUnLockFinished(const QByteArray &folderId, int statusCode)
{
    BasePropagateRemoteDeleteEncrypted::slotFolderUnLockFinished(folderId, statusCode);
    if (statusCode == 200) {
        decryptAndRemoteDelete();
    }
}

void PropagateRemoteDeleteEncryptedRootFolder::slotFetchMetadataJobFinished(int statusCode, const QString &message)
{
    Q_UNUSED(message);
    if (statusCode == 404) {
        // we've eneded up having no metadata, but, _nestedItems is not empty since we went this far, let's proceed with removing the nested items without
        // modifying the metadata
        qCDebug(PROPAGATE_REMOVE_ENCRYPTED_ROOTFOLDER) << "There is no metadata for this folder. Just remove it's nested items.";
        for (auto it = _nestedItems.constBegin(); it != _nestedItems.constEnd(); ++it) {
            deleteNestedRemoteItem(it.key());
        }
        return;
    }

    const auto metadata = folderMetadata();

    if (!metadata || !metadata->isValid()) {
        taskFailed();
        return;
    }

    qCDebug(PROPAGATE_REMOVE_ENCRYPTED_ROOTFOLDER) << "It's a root encrypted folder. Let's remove nested items first.";

    metadata->removeAllEncryptedFiles();

    qCDebug(PROPAGATE_REMOVE_ENCRYPTED_ROOTFOLDER) << "Metadata updated, sending to the server.";
    uploadMetadata(EncryptedFolderMetadataHandler::UploadMode::KeepLock);
}

void PropagateRemoteDeleteEncryptedRootFolder::slotUpdateMetadataJobFinished(int statusCode, const QString &message)
{
    Q_UNUSED(message);
    if (statusCode != 200) {
        taskFailed();
        return;
    }
    for (auto it = _nestedItems.constBegin(); it != _nestedItems.constEnd(); ++it) {
        deleteNestedRemoteItem(it.key());
    }
}

void PropagateRemoteDeleteEncryptedRootFolder::slotDeleteNestedRemoteItemFinished()
{
    auto *deleteJob = qobject_cast<DeleteJob *>(QObject::sender());

    Q_ASSERT(deleteJob);

    if (!deleteJob) {
        return;
    }

    const QString encryptedFileName = deleteJob->property(encryptedFileNamePropertyKey).toString();

    if (!encryptedFileName.isEmpty()) {
        const auto nestedItem = _nestedItems.take(encryptedFileName);

        if (nestedItem.isValid()) {
            if (!_propagator->_journal->deleteFileRecord(nestedItem._path, nestedItem._type == ItemTypeDirectory)) {
                qCWarning(PROPAGATE_REMOVE_ENCRYPTED_ROOTFOLDER) << "Failed to delete file record from local DB" << nestedItem._path;
            }
            _propagator->_journal->commit("Remote Remove");
        }
    }

    QNetworkReply::NetworkError err = deleteJob->reply()->error();

    const auto httpErrorCode = deleteJob->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = deleteJob->responseTimestamp();
    _item->_requestId = deleteJob->requestId();

    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {
        storeFirstError(err);
        storeFirstErrorString(deleteJob->errorString());
        qCWarning(PROPAGATE_REMOVE_ENCRYPTED_ROOTFOLDER) << "Delete nested item finished with error" << err << ".";
    } else if (httpErrorCode != 204 && httpErrorCode != 404) {
        // A 404 reply is also considered a success here: We want to make sure
        // a file is gone from the server. It not being there in the first place
        // is ok. This will happen for files that are in the DB but not on
        // the server or the local file system.

        // Normally we expect "204 No Content"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        storeFirstErrorString(tr("Wrong HTTP code returned by server. Expected 204, but received \"%1 %2\".")
                       .arg(httpErrorCode)
                       .arg(deleteJob->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
        if (_item->_httpErrorCode == 0) {
            _item->_httpErrorCode = httpErrorCode;
        }

        qCWarning(PROPAGATE_REMOVE_ENCRYPTED_ROOTFOLDER) << "Delete nested item finished with error" << httpErrorCode << ".";
    }

    if (_nestedItems.size() == 0) {
        // we wait for all _nestedItems' DeleteJobs to finish, and then - fail if any of those jobs has failed
        if (networkError() != QNetworkReply::NetworkError::NoError || _item->_httpErrorCode != 0) {
            const auto errorCode = (networkError() != QNetworkReply::NetworkError::NoError ? static_cast<int>(networkError()) : _item->_httpErrorCode);
            qCCritical(PROPAGATE_REMOVE_ENCRYPTED_ROOTFOLDER) << "Delete of nested items finished with error" << errorCode << ". Failing the entire sequence.";
            taskFailed();
            return;
        }
        unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Success);
    }
}

void PropagateRemoteDeleteEncryptedRootFolder::deleteNestedRemoteItem(const QString &filename)
{
    qCInfo(PROPAGATE_REMOVE_ENCRYPTED_ROOTFOLDER) << "Deleting nested encrypted remote item" << filename;

    auto deleteJob = new DeleteJob(_propagator->account(), _propagator->fullRemotePath(filename), this);
    deleteJob->setFolderToken(folderToken());
    deleteJob->setProperty(encryptedFileNamePropertyKey, filename);

    connect(deleteJob, &DeleteJob::finishedSignal, this, &PropagateRemoteDeleteEncryptedRootFolder::slotDeleteNestedRemoteItemFinished);

    deleteJob->start();
}

void PropagateRemoteDeleteEncryptedRootFolder::decryptAndRemoteDelete()
{
    auto job = new OCC::SetEncryptionFlagApiJob(_propagator->account(), _item->_fileId, OCC::SetEncryptionFlagApiJob::Clear, this);
    connect(job, &OCC::SetEncryptionFlagApiJob::success, this, [this] (const QByteArray &fileId) {
        Q_UNUSED(fileId);
        deleteRemoteItem(_item->_file);
    });
    connect(job, &OCC::SetEncryptionFlagApiJob::error, this, [this] (const QByteArray &fileId, int httpReturnCode) {
        Q_UNUSED(fileId);
        _item->_httpErrorCode = httpReturnCode;
        taskFailed();
    });
    job->start();
}

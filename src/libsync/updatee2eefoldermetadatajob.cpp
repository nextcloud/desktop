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

#include "updatee2eefoldermetadatajob.h"

#include "account.h"
#include "clientsideencryption.h"
#include "foldermetadata.h"

#include <QLoggingCategory>
#include <QNetworkReply>

namespace OCC {

Q_LOGGING_CATEGORY(lcUpdateFileDropMetadataJob, "nextcloud.sync.propagator.updatee2eefoldermetadatajob", QtInfoMsg)

}

namespace OCC {

UpdateE2eeFolderMetadataJob::UpdateE2eeFolderMetadataJob(OwncloudPropagator *propagator, const SyncFileItemPtr &item, const QString &encryptedRemotePath)
    : PropagatorJob(propagator),
    _item(item),
    _encryptedRemotePath(encryptedRemotePath)
{
}

void UpdateE2eeFolderMetadataJob::start()
{
    Q_ASSERT(_item);
    qCDebug(lcUpdateFileDropMetadataJob) << "Folder is encrypted, let's fetch metadata.";

    SyncJournalFileRecord rec;
    if (!propagator()->_journal->getRootE2eFolderRecord(Utility::fullRemotePathToRemoteSyncRootRelative(_encryptedRemotePath, propagator()->remotePath()), &rec) || !rec.isValid()) {
        unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Failure);
        return;
    }
    _encryptedFolderMetadataHandler.reset(
            new EncryptedFolderMetadataHandler(propagator()->account(), _encryptedRemotePath, propagator()->_journal, rec.path()));

    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::fetchFinished,
            this, &UpdateE2eeFolderMetadataJob::slotFetchMetadataJobFinished);
    _encryptedFolderMetadataHandler->fetchMetadata(EncryptedFolderMetadataHandler::FetchMode::AllowEmptyMetadata);
}

bool UpdateE2eeFolderMetadataJob::scheduleSelfOrChild()
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

PropagatorJob::JobParallelism UpdateE2eeFolderMetadataJob::parallelism() const
{
    return PropagatorJob::JobParallelism::WaitForFinished;
}

void UpdateE2eeFolderMetadataJob::slotFetchMetadataJobFinished(int httpReturnCode, const QString &message)
{
    if (httpReturnCode != 200) {
        qCDebug(lcUpdateFileDropMetadataJob()) << "Error Getting the encrypted metadata.";
        _item->_status = SyncFileItem::FatalError;
        _item->_errorString = message;
        finished(SyncFileItem::FatalError);
        return;
    }

    SyncJournalFileRecord rec;
    if (!propagator()->_journal->getRootE2eFolderRecord(Utility::fullRemotePathToRemoteSyncRootRelative(_encryptedRemotePath, propagator()->remotePath()), &rec)
        || !rec.isValid()) {
        unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Failure);
        return;
    }

    const auto folderMetadata = _encryptedFolderMetadataHandler->folderMetadata();
    if (!folderMetadata || !folderMetadata->isValid() || (!folderMetadata->moveFromFileDropToFiles() && !folderMetadata->encryptedMetadataNeedUpdate())) {
        unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Failure);
        return;
    }

    emit fileDropMetadataParsedAndAdjusted(folderMetadata.data());
    _encryptedFolderMetadataHandler->uploadMetadata();
    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::uploadFinished,
            this, &UpdateE2eeFolderMetadataJob::slotUpdateMetadataFinished);
}

void UpdateE2eeFolderMetadataJob::slotUpdateMetadataFinished(int httpReturnCode, const QString &message)
{
    const auto itemStatus = httpReturnCode != 200 ? SyncFileItem::FatalError : SyncFileItem::Success;
    if (httpReturnCode != 200) {
        _item->_errorString = message;
        qCDebug(lcUpdateFileDropMetadataJob) << "Update metadata error for folder" << _encryptedFolderMetadataHandler->folderId() << "with error" << httpReturnCode << message;
    } else {
        qCDebug(lcUpdateFileDropMetadataJob) << "Uploading of the metadata success, Encrypting the file";
    }
    propagator()->_journal->schedulePathForRemoteDiscovery(_item->_file);
    propagator()->_anotherSyncNeeded = true;
    _item->_status = itemStatus;
    finished(itemStatus);
}

void UpdateE2eeFolderMetadataJob::unlockFolder(const EncryptedFolderMetadataHandler::UnlockFolderWithResult result)
{
    Q_ASSERT(!_encryptedFolderMetadataHandler->isUnlockRunning());
    Q_ASSERT(_item);

    if (_encryptedFolderMetadataHandler->isUnlockRunning()) {
        qCWarning(lcUpdateFileDropMetadataJob) << "Double-call to unlockFolder.";
        return;
    }

    if (result != EncryptedFolderMetadataHandler::UnlockFolderWithResult::Success) {
        _item->_errorString = tr("Failed to update folder metadata.");
    }

    const auto isSuccess = result == EncryptedFolderMetadataHandler::UnlockFolderWithResult::Success;

    const auto itemStatus = isSuccess ? SyncFileItem::Success : SyncFileItem::FatalError;

    if (!_encryptedFolderMetadataHandler->isFolderLocked()) {
        if (isSuccess && _encryptedFolderMetadataHandler->folderMetadata()) {
            _item->_e2eEncryptionStatus = _encryptedFolderMetadataHandler->folderMetadata()->encryptedMetadataEncryptionStatus();
            if (_item->isEncrypted()) {
                _item->_e2eEncryptionServerCapability = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(propagator()->account()->capabilities().clientSideEncryptionVersion());
            }
        }
        finished(itemStatus);
        return;
    }

    qCDebug(lcUpdateFileDropMetadataJob) << "Calling Unlock";
    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::folderUnlocked, [this](const QByteArray &folderId, int httpStatus) {
        if (httpStatus != 200) {
            qCWarning(lcUpdateFileDropMetadataJob) << "Unlock Error" << folderId << httpStatus;
            propagator()->account()->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            _item->_errorString = tr("Failed to unlock encrypted folder.");
            finished(SyncFileItem::FatalError);
            return;
        }

        qCDebug(lcUpdateFileDropMetadataJob) << "Successfully Unlocked";

        if (!_encryptedFolderMetadataHandler->folderMetadata()
            || !_encryptedFolderMetadataHandler->folderMetadata()->isValid()) {
            qCWarning(lcUpdateFileDropMetadataJob) << "Failed to finalize item. Invalid metadata.";
            _item->_errorString = tr("Failed to finalize item.");
            finished(SyncFileItem::FatalError);
            return;
        }

        _item->_e2eEncryptionStatus = _encryptedFolderMetadataHandler->folderMetadata()->encryptedMetadataEncryptionStatus();
        _item->_e2eEncryptionStatusRemote = _encryptedFolderMetadataHandler->folderMetadata()->encryptedMetadataEncryptionStatus();

        finished(SyncFileItem::Success);
    });
    _encryptedFolderMetadataHandler->unlockFolder(result);
}

}

/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "propagateremotedelete.h"
#include "propagateremotedeleteencrypted.h"
#include "propagateremotedeleteencryptedrootfolder.h"
#include "owncloudpropagator_p.h"
#include "account.h"
#include "deletejob.h"
#include "common/asserts.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateRemoteDelete, "nextcloud.sync.propagator.remotedelete", QtInfoMsg)

void PropagateRemoteDelete::start()
{
    qCInfo(lcPropagateRemoteDelete) << "Start propagate remote delete job for" << _item->_file;
    qCInfo(lcPermanentLog) << "delete" << _item->_file << _item->_discoveryResult;

    if (propagator()->_abortRequested)
        return;

    // Partial Delete Logic: Check if this folder has unsynced descendants
    // This prevents data loss when deleting folders that contain selective sync exclusions
    if (_item->isDirectory() && propagator() && propagator()->_journal &&
        propagator()->_journal->hasSelectiveSyncDescendants(_item->_file))
        {
        qCInfo(lcPropagateRemoteDelete) << "Folder" << _item->_file
                 << "has unsynced descendants. Performing partial deletion...";

        // Get all synced descendants that need to be deleted
        _syncedItemsToDelete = propagator()->_journal->getSyncedDescendants(_item->_file);
        _syncedItemsToDelete.append(_item->_file);
        _syncedItemsToKeep = _syncedItemsToDelete;
        _syncedItemsToDelete.removeIf([this](const QString &path){return propagator()->_journal->hasSelectiveSyncDescendants(path);});
        _syncedItemsToDelete.removeIf([this](const QString &path) {
            SyncJournalFileRecord rec;
            assert(propagator()->_journal->getFileRecord(path,&rec));
            int endpos = path.lastIndexOf("/");
            QString parentPath = path.left(endpos);
            if ( rec.isDirectory()
                || propagator()->_journal->hasSelectiveSyncDescendants(parentPath)) {return false;};
            return _syncedItemsToDelete.contains(parentPath);
        });

        // add folders for selectivesync blacklist, required to imitate a proper deletion to the user
        _syncedItemsToKeep.removeIf([this](const QString &path) {
            bool selectiveSynced = propagator()->_journal->hasSelectiveSyncDescendants(path);
            SyncJournalFileRecord rec;
            propagator()->_journal->getFileRecord(path,&rec);
            if (not selectiveSynced || not rec.isDirectory()) {
                return true;
            }
            int endpos = path.lastIndexOf("/");
            QString parentPath = path.left(endpos);
            return _syncedItemsToKeep.contains(parentPath); // deduplicates blacklist entries
        });
        for (QString path: _syncedItemsToKeep) {
            if (!PropagateItemJob::removePathFromSelectiveSyncRecursively(propagator()->_journal, path)) {
                return;
            }
            if (!PropagateItemJob::addPathToSelectiveSync(propagator()->_journal, path)) {
                return;
            }
        }
        if (!_syncedItemsToDelete.isEmpty()) {
            qCInfo(lcPropagateRemoteDelete) << "Partial deletion: deleting" << _syncedItemsToDelete.size()
                     << "synced items while keeping unsynced content";

            // Start partial deletion process
            deleteNextSyncedItem();
            return;
        }
        // No synced children to delete, just skip this operation
        // The folder was never synced locally, so nothing to delete
        qCInfo(lcPropagateRemoteDelete) << "No synced items to delete in folder with unsynced descendants. Skipping.";
        done(SyncFileItem::Success, {}, ErrorCategory::NoError);
        return;
    }

    if (!_item->_encryptedFileName.isEmpty() || _item->isEncrypted()) {
        if (!_item->_encryptedFileName.isEmpty()) {
            _deleteEncryptedHelper = new PropagateRemoteDeleteEncrypted(propagator(), _item, this);
        } else {
            _deleteEncryptedHelper = new PropagateRemoteDeleteEncryptedRootFolder(propagator(), _item, this);
        }
        connect(_deleteEncryptedHelper, &BasePropagateRemoteDeleteEncrypted::finished, this, [this] (bool success) {
            if (!success) {
                SyncFileItem::Status status = SyncFileItem::NormalError;
                if (_deleteEncryptedHelper->networkError() != QNetworkReply::NoError && _deleteEncryptedHelper->networkError() != QNetworkReply::ContentNotFoundError) {
                    status = classifyError(_deleteEncryptedHelper->networkError(), _item->_httpErrorCode, &propagator()->_anotherSyncNeeded);
                }
                done(status, _deleteEncryptedHelper->errorString(), ErrorCategory::GenericError);
            } else {
                done(SyncFileItem::Success, {}, ErrorCategory::NoError);
            }
        });
        _deleteEncryptedHelper->start();
    } else {
        createDeleteJob(_item->_file);
    }
}

void PropagateRemoteDelete::createDeleteJob(const QString &filename)
{
    Q_ASSERT(propagator());
    auto remoteFilename = filename;
    if (_item->_type == ItemType::ItemTypeVirtualFile) {
        if (const auto vfs = propagator()->syncOptions()._vfs; vfs->mode() == Vfs::Mode::WithSuffix) {
            // These are compile-time constants so no need to recreate each time
            static constexpr auto suffixSize = std::string_view(APPLICATION_DOTVIRTUALFILE_SUFFIX).size();
            remoteFilename.chop(suffixSize);
        }
    }

    qCInfo(lcPropagateRemoteDelete) << "Deleting file, local" << _item->_file << "remote" << remoteFilename << "wantsPermanentDeletion" << (_item->_wantsSpecificActions == SyncFileItem::SynchronizationOptions::WantsPermanentDeletion ? "true" : "false");

    auto headers = QMap<QByteArray, QByteArray>{};
    if (_item->_locked == SyncFileItem::LockStatus::LockedItem) {
        headers[QByteArrayLiteral("If")] = (QLatin1String("<") + propagator()->account()->davUrl().toString() + _item->_file + "> (<opaquelocktoken:" + _item->_lockToken.toUtf8() + ">)").toUtf8();
    }
    _job = new DeleteJob(propagator()->account(), propagator()->fullRemotePath(remoteFilename), headers, this);
    _job->setSkipTrashbin(_item->_wantsSpecificActions == SyncFileItem::SynchronizationOptions::WantsPermanentDeletion);
    connect(_job.data(), &DeleteJob::finishedSignal, this, &PropagateRemoteDelete::slotDeleteJobFinished);
    propagator()->_activeJobList.append(this);
    _job->start();
}

void PropagateRemoteDelete::abort(PropagatorJob::AbortType abortType)
{
    if (_job && _job->reply())
        _job->reply()->abort();

    if (abortType == AbortType::Asynchronous) {
        emit abortFinished();
    }
}

void PropagateRemoteDelete::slotDeleteJobFinished()
{
    propagator()->_activeJobList.removeOne(this);

    ASSERT(_job);

    QNetworkReply::NetworkError err = _job->reply()->error();
    const int httpStatus = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_httpErrorCode = httpStatus;
    _item->_responseTimeStamp = _job->responseTimestamp();
    _item->_requestId = _job->requestId();

    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
            &propagator()->_anotherSyncNeeded);

        done(status, _job->errorString(), errorCategoryFromNetworkError(err));
        return;
    }

    // A 404 reply is also considered a success here: We want to make sure
    // a file is gone from the server. It not being there in the first place
    // is ok. This will happen for files that are in the DB but not on
    // the server or the local file system.
    if (httpStatus != 204 && httpStatus != 404) {
        // Normally we expect "204 No Content"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError,
            tr("Wrong HTTP code returned by server. Expected 204, but received \"%1 %2\".")
                .arg(_item->_httpErrorCode)
                .arg(_job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()), ErrorCategory::GenericError);
        return;
    }

    if (!propagator()->_journal->deleteFileRecord(_item->_originalFile, _item->isDirectory())) {
        qCWarning(lcPropagateRemoteDelete) << "could not delete file from local DB" << _item->_originalFile;
        done(SyncFileItem::NormalError, tr("Could not delete file record %1 from local DB").arg(_item->_originalFile), ErrorCategory::GenericError);
        return;
    }

    propagator()->_journal->commit("Remote Remove");

    done(SyncFileItem::Success, {}, ErrorCategory::NoError);
}

void PropagateRemoteDelete::deleteNextSyncedItem()
{
    if (_currentDeleteIndex >= _syncedItemsToDelete.size()) {
        // All synced items have been deleted successfully
        // The unsynced content remains on the server
        qCInfo(lcPropagateRemoteDelete) << "Partial deletion complete. Deleted" << _syncedItemsToDelete.size()
                 << "synced items while preserving unsynced content.";
        done(SyncFileItem::Success, {}, ErrorCategory::NoError);
        return;
    }

    QString itemPath = _syncedItemsToDelete[_currentDeleteIndex];

    qCInfo(lcPropagateRemoteDelete) << "Partial deletion: deleting item" << (_currentDeleteIndex + 1)
             << "of" << _syncedItemsToDelete.size() << ":" << itemPath;

    createPartialDeleteJob(itemPath);
}

void PropagateRemoteDelete::createPartialDeleteJob(const QString &remoteFilename)
{
    Q_ASSERT(propagator());

    auto headers = QMap<QByteArray, QByteArray>{};
    if (_item->_locked == SyncFileItem::LockStatus::LockedItem) {
        headers[QByteArrayLiteral("If")] = (QLatin1String("<") + propagator()->account()->davUrl().toString() + _item->_file + "> (<opaquelocktoken:" + _item->_lockToken.toUtf8() + ">)").toUtf8();
    }

    _job = new DeleteJob(propagator()->account(), propagator()->fullRemotePath(remoteFilename), headers, this);
    _job->setSkipTrashbin(_item->_wantsSpecificActions == SyncFileItem::SynchronizationOptions::WantsPermanentDeletion);
    connect(_job.data(), &DeleteJob::finishedSignal, this, &PropagateRemoteDelete::slotPartialDeleteJobFinished);
    propagator()->_activeJobList.append(this);
    _job->start();
}

void PropagateRemoteDelete::slotPartialDeleteJobFinished()
{
    propagator()->_activeJobList.removeOne(this);

    ASSERT(_job);

    QNetworkReply::NetworkError err = _job->reply()->error();
    const int httpStatus = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Check if it was already deleted (404 is ok for partial deletion)
    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {
        qCWarning(lcPropagateRemoteDelete) << "Partial deletion failed for item:" << _currentDeleteIndex << err;
        done(SyncFileItem::SoftError, _job->errorString(), ErrorCategory::GenericError);
        return;
    }

    // Delete the file record from the local database
    QString itemPath = _syncedItemsToDelete[_currentDeleteIndex];
    // todo: do I need an if(isDirectory) then recursively = true else false?
    if (!propagator()->_journal->deleteFileRecord(itemPath, false)) {
        qCWarning(lcPropagateRemoteDelete) << "Could not delete file record from local DB:" << itemPath;
    }

    // Move to next item
    _currentDeleteIndex++;
    deleteNextSyncedItem();
}
}

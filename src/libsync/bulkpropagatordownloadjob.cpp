/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "bulkpropagatordownloadjob.h"

#include "syncfileitem.h"
#include "syncengine.h"
#include "common/syncjournaldb.h"
#include "propagatorjobs.h"
#include "filesystem.h"
#include "account.h"
#include "propagatedownloadencrypted.h"

#include <QDir>

namespace OCC {

Q_LOGGING_CATEGORY(lcBulkPropagatorDownloadJob, "nextcloud.sync.propagator.bulkdownload", QtInfoMsg)

BulkPropagatorDownloadJob::BulkPropagatorDownloadJob(OwncloudPropagator *propagator,
                                                     PropagateDirectory *parentDirJob)
    : PropagatorJob{propagator}
    , _filesToDownload{}
    , _parentDirJob{parentDirJob}
{
}

void BulkPropagatorDownloadJob::addDownloadItem(const SyncFileItemPtr &item)
{
    Q_ASSERT(item->isDirectory() || item->_type == ItemTypeVirtualFileDehydration || item->_type == ItemTypeVirtualFile);
    if (item->isDirectory() || (item->_type != ItemTypeVirtualFileDehydration && item->_type != ItemTypeVirtualFile)) {
        qCWarning(lcBulkPropagatorDownloadJob) << "Failed to process bulk download for a non-virtual file" << item->_originalFile;
        return;
    }
    _filesToDownload.push_back(item);
}

bool BulkPropagatorDownloadJob::scheduleSelfOrChild()
{
    if (_filesToDownload.empty()) {
        return false;
    }

    _state = Running;

    start();

    _state = Finished;

    return false;
}

PropagatorJob::JobParallelism BulkPropagatorDownloadJob::parallelism() const
{
    return PropagatorJob::JobParallelism::FullParallelism;
}

void BulkPropagatorDownloadJob::finalizeOneFile(const SyncFileItemPtr &file)
{
    emit propagator()->itemCompleted(file, ErrorCategory::GenericError);
}

void BulkPropagatorDownloadJob::start()
{
    if (propagator()->_abortRequested) {
        abortWithError({}, SyncFileItem::NormalError, {});
        return;
    }

    for (const auto &fileToDownload : std::as_const(_filesToDownload)) {
        Q_ASSERT(fileToDownload->_type == ItemTypeVirtualFile);

        if (propagator()->localFileNameClash(fileToDownload->_file)) {
            fileToDownload->_status = SyncFileItem::FileNameClash;
            finalizeOneFile(fileToDownload);
            qCWarning(lcBulkPropagatorDownloadJob) << "File" << QDir::toNativeSeparators(fileToDownload->_file) << "can not be downloaded because of a local file name clash!";
            abortWithError(fileToDownload, SyncFileItem::FileNameClash, tr("File %1 can not be downloaded because of a local file name clash!").arg(QDir::toNativeSeparators(fileToDownload->_file)));
            return;
        }
    }

    const auto &vfs = propagator()->syncOptions()._vfs;
    Q_ASSERT(vfs && vfs->mode() == Vfs::WindowsCfApi);

    const auto r = vfs->createPlaceholders(_filesToDownload);

    if (!r) {
        qCCritical(lcBulkPropagatorDownloadJob) << "Could not create placholders:" << r.error();
        for (const auto &fileToDownload : std::as_const(_filesToDownload)) {
            fileToDownload->_status = SyncFileItem::NormalError;
            finalizeOneFile(fileToDownload);
        }
        abortWithError({}, SyncFileItem::NormalError, r.error());
        return;
    }

    for (const auto &fileToDownload : std::as_const(_filesToDownload)) {
        if (!updateMetadata(fileToDownload)) {
            abortWithError(fileToDownload, SyncFileItem::NormalError, tr("Unable to update metadata of new file %1.", "error with update metadata of new Win VFS file").arg(fileToDownload->_file));
            return;
        }

        if (!fileToDownload->_remotePerm.isNull() && !fileToDownload->_remotePerm.hasPermission(RemotePermissions::CanWrite)) {
            // make sure ReadOnly flag is preserved for placeholder, similarly to regular files
            FileSystem::setFileReadOnly(propagator()->fullLocalPath(fileToDownload->_file), true);
        }
        fileToDownload->_status = SyncFileItem::Success;
        finalizeOneFile(fileToDownload);
    }

    _filesToDownload.clear();

    done(SyncFileItem::Success);
}

bool BulkPropagatorDownloadJob::updateMetadata(const SyncFileItemPtr &item)
{
    const auto fullFileName = propagator()->fullLocalPath(item->_file);
    const auto updateMetadataFlags = Vfs::UpdateMetadataTypes{Vfs::UpdateMetadataType::AllMetadata};
    const auto result = propagator()->updateMetadata(*item, updateMetadataFlags);
    if (!result) {
        abortWithError(item, SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()));
        return false;
    } else if (*result == Vfs::ConvertToPlaceholderResult::Locked) {
        abortWithError(item, SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(item->_file));
        return false;
    }

    propagator()->_journal->commit("download file start2");

    const auto isLockOwnedByCurrentUser = item->_lockOwnerId == propagator()->account()->davUser();

    const auto isUserLockOwnedByCurrentUser = (item->_lockOwnerType == SyncFileItem::LockOwnerType::UserLock && isLockOwnedByCurrentUser);
    const auto isTokenLockOwnedByCurrentUser = (item->_lockOwnerType == SyncFileItem::LockOwnerType::TokenLock && isLockOwnedByCurrentUser);

    if (item->_locked == SyncFileItem::LockStatus::LockedItem && !isUserLockOwnedByCurrentUser && !isTokenLockOwnedByCurrentUser) {
        qCDebug(lcBulkPropagatorDownloadJob()) << fullFileName << "file is locked: making it read only";
        FileSystem::setFileReadOnly(fullFileName, true);
    } else {
        qCDebug(lcBulkPropagatorDownloadJob()) << fullFileName << "file is not locked: making it" << ((!item->_remotePerm.isNull() && !item->_remotePerm.hasPermission(RemotePermissions::CanWrite))
            ? "read only"
            : "read write");
        FileSystem::setFileReadOnlyWeak(fullFileName, (!item->_remotePerm.isNull() && !item->_remotePerm.hasPermission(RemotePermissions::CanWrite)));
    }
    return true;
}

void BulkPropagatorDownloadJob::done(const SyncFileItem::Status status)
{
    emit finished(status);
}

void BulkPropagatorDownloadJob::abortWithError(SyncFileItemPtr item, SyncFileItem::Status status, const QString &error)
{
    qCInfo(lcBulkPropagatorDownloadJob) << "finished with status" << status << error;
    abort(AbortType::Synchronous);
    if (item) {
        item->_errorString = error;
        item->_status = status;
        emit propagator()->itemCompleted(item, ErrorCategory::GenericError);
    }
    done(status);
}

}

/*
 * Copyright (C) 2024 by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "bulkpropagatordownloadjob.h"

#include "owncloudpropagator_p.h"
#include "syncfileitem.h"
#include "syncengine.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "propagatorjobs.h"
#include "filesystem.h"
#include "account.h"
#include "networkjobs.h"
#include "propagatedownloadencrypted.h"

#include <QDir>

namespace OCC {

Q_LOGGING_CATEGORY(lcBulkPropagatorDownloadJob, "nextcloud.sync.propagator.bulkdownload", QtInfoMsg)

BulkPropagatorDownloadJob::BulkPropagatorDownloadJob(OwncloudPropagator *propagator,
                                                     PropagateDirectory *parentDirJob,
                                                     const std::vector<SyncFileItemPtr> &items)
    : PropagatorJob(propagator)
    , _filesToDownload(items)
    , _parentDirJob(parentDirJob)
{
}

namespace
{
static QString makeRecallFileName(const QString &fn)
{
    QString recallFileName(fn);
    // Add _recall-XXXX  before the extension.
    int dotLocation = recallFileName.lastIndexOf('.');
    // If no extension, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
    if (dotLocation <= recallFileName.lastIndexOf('/') + 1) {
        dotLocation = recallFileName.size();
    }

    QString timeString = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss");
    recallFileName.insert(dotLocation, "_.sys.admin#recall#-" + timeString);

    return recallFileName;
}

void handleRecallFile(const QString &filePath, const QString &folderPath, SyncJournalDb &journal)
{
    qCDebug(lcBulkPropagatorDownloadJob) << "handleRecallFile: " << filePath;

    FileSystem::setFileHidden(filePath, true);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcBulkPropagatorDownloadJob) << "Could not open recall file" << file.errorString();
        return;
    }
    QFileInfo existingFile(filePath);
    QDir baseDir = existingFile.dir();

    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        line.chop(1); // remove trailing \n

        QString recalledFile = QDir::cleanPath(baseDir.filePath(line));
        if (!recalledFile.startsWith(folderPath) || !recalledFile.startsWith(baseDir.path())) {
            qCWarning(lcBulkPropagatorDownloadJob) << "Ignoring recall of " << recalledFile;
            continue;
        }

        // Path of the recalled file in the local folder
        QString localRecalledFile = recalledFile.mid(folderPath.size());

        SyncJournalFileRecord record;
        if (!journal.getFileRecord(localRecalledFile, &record) || !record.isValid()) {
            qCWarning(lcBulkPropagatorDownloadJob) << "No db entry for recall of" << localRecalledFile;
            continue;
        }

        qCInfo(lcBulkPropagatorDownloadJob) << "Recalling" << localRecalledFile << "Checksum:" << record._checksumHeader;

        QString targetPath = makeRecallFileName(recalledFile);

        qCDebug(lcBulkPropagatorDownloadJob) << "Copy recall file: " << recalledFile << " -> " << targetPath;
        // Remove the target first, QFile::copy will not overwrite it.
        FileSystem::remove(targetPath);
        QFile::copy(recalledFile, targetPath);
    }
}
}

void BulkPropagatorDownloadJob::addDownloadItem(const SyncFileItemPtr &item)
{
    Q_ASSERT(item->isDirectory() || item->_type == ItemTypeVirtualFileDehydration || item->_type == ItemTypeVirtualFile);
    if (item->isDirectory() || item->_type != ItemTypeVirtualFileDehydration && item->_type != ItemTypeVirtualFile) {
        qCDebug(lcBulkPropagatorDownloadJob) << "Failed to process bulk download for a non-virtual file" << item->_originalFile;
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

    for (const auto fileToDownload : _filesToDownload) {
        qCDebug(lcBulkPropagatorDownloadJob) << "Scheduling bulk propagator job:" << this << "and starting download of item"
                                             << "with file:" << fileToDownload->_file << "with size:" << fileToDownload->_size;
        _filesDownloading.push_back(fileToDownload);
        start(fileToDownload);
    }

    _filesToDownload.clear();

    checkPropagationIsDone();

    return true;
}

PropagatorJob::JobParallelism BulkPropagatorDownloadJob::parallelism() const
{
    return PropagatorJob::JobParallelism::FullParallelism;
}

void BulkPropagatorDownloadJob::startAfterIsEncryptedIsChecked(const SyncFileItemPtr &item)
{
    const auto &vfs = propagator()->syncOptions()._vfs;
    Q_ASSERT(vfs && vfs->mode() == Vfs::WindowsCfApi);
    Q_ASSERT(item->_type == ItemTypeVirtualFileDehydration || item->_type == ItemTypeVirtualFile);

    if (propagator()->localFileNameClash(item->_file)) {
        _parentDirJob->appendTask(item);
        finalizeOneFile(item);
        return;
    }

    // For virtual files just dehydrate or create the placeholder and be done
    if (item->_type == ItemTypeVirtualFileDehydration) {
        const auto fsPath = propagator()->fullLocalPath(item->_file);
        if (!FileSystem::verifyFileUnchanged(fsPath, item->_previousSize, item->_previousModtime)) {
            propagator()->_anotherSyncNeeded = true;
            item->_errorString = tr("File has changed since discovery");
            abortWithError(item, SyncFileItem::SoftError, tr("File has changed since discovery"));
            return;
        }
        qCDebug(lcBulkPropagatorDownloadJob) << "dehydrating file" << item->_file;
        const auto r = vfs->dehydratePlaceholder(*item);
        if (!r) {
            qCCritical(lcBulkPropagatorDownloadJob) << "Could not dehydrate a file" << QDir::toNativeSeparators(item->_file) << ":" <<  r.error();
            abortWithError(item, SyncFileItem::NormalError, r.error());
            return;
        }
        if (!propagator()->_journal->deleteFileRecord(item->_originalFile)) {
            qCWarning(lcBulkPropagatorDownloadJob) << "could not delete file from local DB" << item->_originalFile;
            abortWithError(item, SyncFileItem::NormalError, tr("Could not delete file record %1 from local DB").arg(item->_originalFile));
            return;
        }
    } else if (item->_type == ItemTypeVirtualFile) {
        qCDebug(lcBulkPropagatorDownloadJob) << "creating virtual file" << item->_file;
        const auto r = vfs->createPlaceholder(*item);
        if (!r) {
            qCCritical(lcBulkPropagatorDownloadJob) << "Could not create a placholder for a file" << QDir::toNativeSeparators(item->_file) << ":" << r.error();
            abortWithError(item, SyncFileItem::NormalError, r.error());
            return;
        }
    } else {
        // we should never get here, as BulkPropagatorDownloadJob must only ever be instantiated and only contain virtual files
        qCCritical(lcBulkPropagatorDownloadJob) << "File" << QDir::toNativeSeparators(item->_file) << "can not be downloaded because it is non virtual!";
        abortWithError(item, SyncFileItem::NormalError, tr("File %1 can not be downloaded because it is non virtual!").arg(QDir::toNativeSeparators(item->_file)));
        return;
    }
    
    if (!updateMetadata(item)) {
        return;
    }

    if (!item->_remotePerm.isNull() && !item->_remotePerm.hasPermission(RemotePermissions::CanWrite)) {
        // make sure ReadOnly flag is preserved for placeholder, similarly to regular files
        FileSystem::setFileReadOnly(propagator()->fullLocalPath(item->_file), true);
    }
    finalizeOneFile(item);
}

void BulkPropagatorDownloadJob::finalizeOneFile(const SyncFileItemPtr &file)
{
    const auto foundIt = std::find_if(std::cbegin(_filesDownloading), std::cend(_filesDownloading), [&file](const auto &fileDownloading) {
        return fileDownloading == file;
    });
    if (foundIt != std::cend(_filesDownloading)) {
        emit propagator()->itemCompleted(file, ErrorCategory::GenericError);
        _filesDownloading.erase(foundIt);
    }
    checkPropagationIsDone();
}

void BulkPropagatorDownloadJob::checkPropagationIsDone()
{
    if (_filesToDownload.empty()  && _filesDownloading.empty()) {
        qCInfo(lcBulkPropagatorDownloadJob) << "finished with status" << SyncFileItem::Status::Success;
        emit finished(SyncFileItem::Status::Success);
        propagator()->scheduleNextJob();
    }
}

void BulkPropagatorDownloadJob::start(const SyncFileItemPtr &item)
{
    if (propagator()->_abortRequested) {
        return;
    }

    qCDebug(lcBulkPropagatorDownloadJob) << item->_file << propagator()->_activeJobList.count();

    const auto path = item->_file;
    const auto slashPosition = path.lastIndexOf('/');
    const auto parentPath = slashPosition >= 0 ? path.left(slashPosition) : QString();

    SyncJournalFileRecord parentRec;
    if (!propagator()->_journal->getFileRecord(parentPath, &parentRec)) {
        qCWarning(lcBulkPropagatorDownloadJob) << "could not get file from local DB" << parentPath;
        abortWithError(item, SyncFileItem::NormalError, tr("could not get file %1 from local DB").arg(parentPath));
        return;
    }
 
    if (!propagator()->account()->capabilities().clientSideEncryptionAvailable() || !parentRec.isValid() || !parentRec.isE2eEncrypted()) {
        startAfterIsEncryptedIsChecked(item);
    } else {
        _downloadEncryptedHelper = new PropagateDownloadEncrypted(propagator(), parentPath, item, this);
        connect(_downloadEncryptedHelper, &PropagateDownloadEncrypted::fileMetadataFound, [this, &item] {
            startAfterIsEncryptedIsChecked(item);
        });
        connect(_downloadEncryptedHelper, &PropagateDownloadEncrypted::failed, [this, &item] {
            abortWithError(
                item,
                SyncFileItem::NormalError,
                tr("File %1 cannot be downloaded because encryption information is missing.").arg(QDir::toNativeSeparators(item->_file)));
        });
        _downloadEncryptedHelper->start();
    }
}

bool BulkPropagatorDownloadJob::updateMetadata(const SyncFileItemPtr &item)
{
    const auto fn = propagator()->fullLocalPath(item->_file);
    const auto result = propagator()->updateMetadata(*item);
    if (!result) {
        abortWithError(item, SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()));
        return false;
    } else if (*result == Vfs::ConvertToPlaceholderResult::Locked) {
        abortWithError(item, SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(item->_file));
        return false;
    }

    propagator()->_journal->commit("download file start2");

    // handle the special recall file
    if (!item->_remotePerm.hasPermission(RemotePermissions::IsShared)
        && (item->_file == QLatin1String(".sys.admin#recall#") || item->_file.endsWith(QLatin1String("/.sys.admin#recall#")))) {
        handleRecallFile(fn, propagator()->localPath(), *propagator()->_journal);
    }

    const auto isLockOwnedByCurrentUser = item->_lockOwnerId == propagator()->account()->davUser();

    const auto isUserLockOwnedByCurrentUser = (item->_lockOwnerType == SyncFileItem::LockOwnerType::UserLock && isLockOwnedByCurrentUser);
    const auto isTokenLockOwnedByCurrentUser = (item->_lockOwnerType == SyncFileItem::LockOwnerType::TokenLock && isLockOwnedByCurrentUser);

    if (item->_locked == SyncFileItem::LockStatus::LockedItem && !isUserLockOwnedByCurrentUser && !isTokenLockOwnedByCurrentUser) {
        qCDebug(lcBulkPropagatorDownloadJob()) << fn << "file is locked: making it read only";
        FileSystem::setFileReadOnly(fn, true);
    } else {
        qCDebug(lcBulkPropagatorDownloadJob()) << fn << "file is not locked: making it" << ((!item->_remotePerm.isNull() && !item->_remotePerm.hasPermission(RemotePermissions::CanWrite))
            ? "read only"
            : "read write");
        FileSystem::setFileReadOnlyWeak(fn, (!item->_remotePerm.isNull() && !item->_remotePerm.hasPermission(RemotePermissions::CanWrite)));
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

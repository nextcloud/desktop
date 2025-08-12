/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "bulkpropagatordownloadjob.h"

#include "syncfileitem.h"
#include "syncengine.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
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

namespace
{
static QString makeRecallFileName(const QString &fn)
{
    auto recallFileName(fn);
    // Add _recall-XXXX  before the extension.
    auto dotLocation = recallFileName.lastIndexOf('.');
    // If no extension, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
    if (dotLocation <= recallFileName.lastIndexOf('/') + 1) {
        dotLocation = recallFileName.size();
    }

    const auto &timeString = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss");
    recallFileName.insert(dotLocation, "_.sys.admin#recall#-" + timeString);

    return recallFileName;
}

void handleRecallFile(const QString &filePath, const QString &folderPath, SyncJournalDb &journal)
{
    qCDebug(lcBulkPropagatorDownloadJob) << "handleRecallFile: " << filePath;

    FileSystem::setFileHidden(filePath, true);

    auto file = QFile{filePath};
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcBulkPropagatorDownloadJob) << "Could not open recall file" << file.errorString();
        return;
    }
    const auto existingFile = QFileInfo{filePath};
    const auto &baseDir = existingFile.dir();

    while (!file.atEnd()) {
        auto line = file.readLine();
        line.chop(1); // remove trailing \n

        const auto &recalledFile = QDir::cleanPath(baseDir.filePath(line));
        if (!recalledFile.startsWith(folderPath) || !recalledFile.startsWith(baseDir.path())) {
            qCWarning(lcBulkPropagatorDownloadJob) << "Ignoring recall of " << recalledFile;
            continue;
        }

        // Path of the recalled file in the local folder
        const auto &localRecalledFile = recalledFile.mid(folderPath.size());

        auto record = SyncJournalFileRecord{};
        if (!journal.getFileRecord(localRecalledFile, &record) || !record.isValid()) {
            qCWarning(lcBulkPropagatorDownloadJob) << "No db entry for recall of" << localRecalledFile;
            continue;
        }

        qCInfo(lcBulkPropagatorDownloadJob) << "Recalling" << localRecalledFile << "Checksum:" << record._checksumHeader;

        const auto &targetPath = makeRecallFileName(recalledFile);

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

    for (const auto &fileToDownload : std::as_const(_filesToDownload)) {
        Q_ASSERT(fileToDownload->_type == ItemTypeVirtualFileDehydration || fileToDownload->_type == ItemTypeVirtualFile);

        if (propagator()->localFileNameClash(fileToDownload->_file)) {
            _parentDirJob->appendTask(fileToDownload);
            finalizeOneFile(fileToDownload);
            qCCritical(lcBulkPropagatorDownloadJob) << "File" << QDir::toNativeSeparators(fileToDownload->_file) << "can not be downloaded because it is non virtual!";
            abortWithError(fileToDownload, SyncFileItem::NormalError, tr("File %1 cannot be downloaded because it is non virtual!").arg(QDir::toNativeSeparators(fileToDownload->_file)));
            return false;
        }
    }

    const auto &vfs = propagator()->syncOptions()._vfs;
    Q_ASSERT(vfs && vfs->mode() == Vfs::WindowsCfApi);

    const auto r = vfs->createPlaceholders(_filesToDownload);

    for (const auto &fileToDownload : std::as_const(_filesToDownload)) {
        if (!updateMetadata(fileToDownload)) {
            return false;
        }

        if (!fileToDownload->_remotePerm.isNull() && !fileToDownload->_remotePerm.hasPermission(RemotePermissions::CanWrite)) {
            // make sure ReadOnly flag is preserved for placeholder, similarly to regular files
            FileSystem::setFileReadOnly(propagator()->fullLocalPath(fileToDownload->_file), true);
        }
        finalizeOneFile(fileToDownload);
    }

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

    if (item->_type == ItemTypeVirtualFile) {
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
        abortWithError(item, SyncFileItem::NormalError, tr("File %1 cannot be downloaded because it is non virtual!").arg(QDir::toNativeSeparators(item->_file)));
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
    const auto foundIt = std::find_if(std::cbegin(_filesToDownload), std::cend(_filesToDownload), [&file](const auto &fileDownloading) {
        return fileDownloading == file;
    });
    if (foundIt != std::cend(_filesToDownload)) {
        emit propagator()->itemCompleted(file, ErrorCategory::GenericError);
        _filesToDownload.erase(foundIt);
    }
    checkPropagationIsDone();
}

void BulkPropagatorDownloadJob::checkPropagationIsDone()
{
    if (_filesToDownload.empty()  && _filesToDownload.empty()) {
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

    const auto &path = item->_file;
    const auto slashPosition = path.lastIndexOf('/');
    const auto &parentPath = slashPosition >= 0 ? path.left(slashPosition) : QString();

    auto parentRec = SyncJournalFileRecord{};
    if (!propagator()->_journal->getFileRecord(parentPath, &parentRec)) {
        qCWarning(lcBulkPropagatorDownloadJob) << "could not get file from local DB" << parentPath;
        abortWithError(item, SyncFileItem::NormalError, tr("Could not get file %1 from local DB").arg(parentPath));
        return;
    }
 
    if (!propagator()->account()->capabilities().clientSideEncryptionAvailable() || !parentRec.isValid() || !parentRec.isE2eEncrypted()) {
        startAfterIsEncryptedIsChecked(item);
    } else {
        Q_ASSERT(false);
        qCCritical(lcBulkPropagatorDownloadJob()) << "no encrypted fiels should be created via bulk download";
    }
}

bool BulkPropagatorDownloadJob::updateMetadata(const SyncFileItemPtr &item)
{
    const auto fullFileName = propagator()->fullLocalPath(item->_file);
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
        handleRecallFile(fullFileName, propagator()->localPath(), *propagator()->_journal);
    }

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

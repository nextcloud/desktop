/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "owncloudpropagator.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "propagatedownload.h"
#include "propagateupload.h"
#include "propagateuploadtus.h"
#include "propagateremotedelete.h"
#include "propagateremotemove.h"
#include "propagateremotemkdir.h"
#include "propagatorjobs.h"
#include "filesystem.h"
#include "common/utility.h"
#include "account.h"
#include "common/asserts.h"
#include "discoveryphase.h"

#ifdef Q_OS_WIN
#include <windef.h>
#include <winbase.h>
#endif

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QObject>
#include <QStack>
#include <QTimer>
#include <QTimerEvent>
#include <qmath.h>

namespace OCC {

Q_LOGGING_CATEGORY(lcPropagator, "sync.propagator", QtInfoMsg)
Q_LOGGING_CATEGORY(lcDirectory, "sync.propagator.directory", QtInfoMsg)

qint64 criticalFreeSpaceLimit()
{
    qint64 value = 50 * 1000 * 1000LL;

    static bool hasEnv = false;
    static qint64 env = qgetenv("OWNCLOUD_CRITICAL_FREE_SPACE_BYTES").toLongLong(&hasEnv);
    if (hasEnv) {
        value = env;
    }

    return qBound(0LL, value, freeSpaceLimit());
}

qint64 freeSpaceLimit()
{
    qint64 value = 250 * 1000 * 1000LL;

    static bool hasEnv = false;
    static qint64 env = qgetenv("OWNCLOUD_FREE_SPACE_BYTES").toLongLong(&hasEnv);
    if (hasEnv) {
        value = env;
    }

    return value;
}

OwncloudPropagator::~OwncloudPropagator()
{
}


int OwncloudPropagator::maximumActiveTransferJob()
{
    if (_downloadLimit != 0
        || _uploadLimit != 0
        || !_syncOptions._parallelNetworkJobs) {
        // disable parallelism when there is a network limit.
        return 1;
    }
    return qMin(3, qCeil(_syncOptions._parallelNetworkJobs / 2.));
}

/* The maximum number of active jobs in parallel  */
int OwncloudPropagator::hardMaximumActiveJob()
{
    if (!_syncOptions._parallelNetworkJobs)
        return 1;
    return _syncOptions._parallelNetworkJobs;
}

PropagateItemJob::~PropagateItemJob()
{
    if (auto p = propagator()) {
        // Normally, every job should clean itself from the _activeJobList. So this should not be
        // needed. But if a job has a bug or is deleted before the network jobs signal get received,
        // we might risk end up with dangling pointer in the list which may cause crashes.
        p->_activeJobList.removeAll(this);
    }
}

bool PropagateItemJob::scheduleSelfOrChild()
{
    if (_state != NotYetStarted) {
        return false;
    }
    qCInfo(lcPropagator) << "Starting" << _item->_instruction << "propagation of" << _item->destination() << "by" << this;

    _state = Running;
    if (thread() != QApplication::instance()->thread()) {
        QMetaObject::invokeMethod(this, &PropagateItemJob::start); // We could be in a different thread (neon jobs)
    } else {
        start();
    }
    return true;
}

static qint64 getMinBlacklistTime()
{
    return qMax(qEnvironmentVariableIntValue("OWNCLOUD_BLACKLIST_TIME_MIN"),
        25); // 25 seconds
}

static qint64 getMaxBlacklistTime()
{
    int v = qEnvironmentVariableIntValue("OWNCLOUD_BLACKLIST_TIME_MAX");
    if (v > 0)
        return v;
    return 24 * 60 * 60; // 1 day
}

/** Creates a blacklist entry, possibly taking into account an old one.
 *
 * The old entry may be invalid, then a fresh entry is created.
 */
static SyncJournalErrorBlacklistRecord createBlacklistEntry(
    const SyncJournalErrorBlacklistRecord &old, const SyncFileItem &item)
{
    SyncJournalErrorBlacklistRecord entry;
    entry._file = item._file;
    entry._errorString = item._errorString;
    entry._lastTryModtime = item._modtime;
    entry._lastTryEtag = item._etag;
    entry._lastTryTime = Utility::qDateTimeToTime_t(QDateTime::currentDateTimeUtc());
    entry._renameTarget = item._renameTarget;
    entry._retryCount = old._retryCount + 1;
    entry._requestId = item._requestId;

    static qint64 minBlacklistTime(getMinBlacklistTime());
    static qint64 maxBlacklistTime(qMax(getMaxBlacklistTime(), minBlacklistTime));

    // The factor of 5 feels natural: 25s, 2 min, 10 min, ~1h, ~5h, ~24h
    entry._ignoreDuration = old._ignoreDuration * 5;

    if (item._httpErrorCode == 403) {
        qCWarning(lcPropagator) << "Probably firewall error: " << item._httpErrorCode << ", blacklisting up to 1h only";
        entry._ignoreDuration = qMin(entry._ignoreDuration, qint64(60 * 60));

    } else if (item._httpErrorCode == 413 || item._httpErrorCode == 415) {
        qCWarning(lcPropagator) << "Fatal Error condition" << item._httpErrorCode << ", maximum blacklist ignore time!";
        entry._ignoreDuration = maxBlacklistTime;
    }

    entry._ignoreDuration = qBound(minBlacklistTime, entry._ignoreDuration, maxBlacklistTime);

    if (item._status == SyncFileItem::SoftError) {
        // Track these errors, but don't actively suppress them.
        entry._ignoreDuration = 0;
    }

    if (item._httpErrorCode == 507) {
        entry._errorCategory = SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage;
    }

    return entry;
}

/** Updates, creates or removes a blacklist entry for the given item.
 *
 * May adjust the status or item._errorString.
 */
static void blacklistUpdate(SyncJournalDb *journal, SyncFileItem &item)
{
    SyncJournalErrorBlacklistRecord oldEntry = journal->errorBlacklistEntry(item._file);

    const bool mayBlacklist = (item._status == SyncFileItem::NormalError)
        || ((item._status == SyncFileItem::SoftError
                || item._status == SyncFileItem::DetailError)
            && item._httpErrorCode != 0 // or non-local error
        );

    // No new entry? Possibly remove the old one, then done.
    if (!mayBlacklist) {
        if (oldEntry.isValid()) {
            journal->wipeErrorBlacklistEntry(item._file);
        }
        return;
    }

    auto newEntry = createBlacklistEntry(oldEntry, item);
    journal->setErrorBlacklistEntry(newEntry);

    // Suppress the error if it was and continues to be blacklisted.
    // An ignoreDuration of 0 mean we're tracking the error, but not actively
    // suppressing it.
    if (item._hasBlacklistEntry && newEntry._ignoreDuration > 0) {
        item._status = SyncFileItem::BlacklistedError;

        qCInfo(lcPropagator) << "blacklisting " << item._file
                             << " for " << newEntry._ignoreDuration
                             << ", retry count " << newEntry._retryCount;

        return;
    }

    // Some soft errors might become louder on repeat occurrence
    if (item._status == SyncFileItem::SoftError
        && newEntry._retryCount > 1) {
        qCWarning(lcPropagator) << "escalating soft error on " << item._file
                                << " to normal error, " << item._httpErrorCode;
        item._status = SyncFileItem::NormalError;
        return;
    }
}

void PropagateItemJob::done(SyncFileItem::Status statusArg, const QString &errorString)
{
    // Duplicate calls to done() are a logic error
    OC_ENFORCE(_state != Finished);
    _state = Finished;

    _item->_status = statusArg;

    if (_item->_isRestoration) {
        if (_item->_status == SyncFileItem::Success
            || _item->_status == SyncFileItem::Conflict) {
            _item->_status = SyncFileItem::Restoration;
        } else {
            _item->_errorString += tr("; Restoration Failed: %1").arg(errorString);
        }
    } else {
        if (_item->_errorString.isEmpty()) {
            _item->_errorString = errorString;
        }
    }

    if (propagator()->_abortRequested && (_item->_status == SyncFileItem::NormalError
                                          || _item->_status == SyncFileItem::FatalError)) {
        // an abort request is ongoing. Change the status to Soft-Error
        _item->_status = SyncFileItem::SoftError;
    }

    // Blacklist handling
    switch (_item->_status) {
    case SyncFileItem::SoftError:
    case SyncFileItem::FatalError:
    case SyncFileItem::NormalError:
    case SyncFileItem::DetailError:
        // Check the blacklist, possibly adjusting the item (including its status)
        blacklistUpdate(propagator()->_journal, *_item);
        break;
    case SyncFileItem::Success:
    case SyncFileItem::Restoration:
        if (_item->_hasBlacklistEntry) {
            // wipe blacklist entry.
            propagator()->_journal->wipeErrorBlacklistEntry(_item->_file);
            // remove a blacklist entry in case the file was moved.
            if (_item->_originalFile != _item->_file) {
                propagator()->_journal->wipeErrorBlacklistEntry(_item->_originalFile);
            }
        }
        break;
    case SyncFileItem::Conflict:
    case SyncFileItem::FileIgnored:
    case SyncFileItem::NoStatus:
    case SyncFileItem::BlacklistedError:
        // nothing
        break;
    }

    if (_item->hasErrorStatus())
        qCWarning(lcPropagator) << "Could not complete propagation of" << _item->destination() << "by" << this << "with status" << _item->_status << "and error:" << _item->_errorString;
    else
        qCInfo(lcPropagator) << "Completed propagation of" << _item->destination() << "by" << this << "with status" << _item->_status;
    emit propagator()->itemCompleted(_item);
    emit finished(_item->_status);

    if (_item->_status == SyncFileItem::FatalError) {
        // Abort all remaining jobs.
        propagator()->abort();
    }
}

// ================================================================================

PropagateItemJob *OwncloudPropagator::createJob(const SyncFileItemPtr &item)
{
    bool deleteExisting = item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE;
    switch (item->_instruction) {
    case CSYNC_INSTRUCTION_REMOVE:
        if (item->_direction == SyncFileItem::Down)
            return new PropagateLocalRemove(this, item);
        else
            return new PropagateRemoteDelete(this, item);
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_TYPE_CHANGE:
    case CSYNC_INSTRUCTION_CONFLICT:
        if (item->isDirectory()) {
            // CONFLICT has _direction == None
            if (item->_direction != SyncFileItem::Up) {
                auto job = new PropagateLocalMkdir(this, item);
                job->setDeleteExistingFile(deleteExisting);
                return job;
            } else {
                auto job = new PropagateRemoteMkdir(this, item);
                job->setDeleteExisting(deleteExisting);
                return job;
            }
        } //fall through
    case CSYNC_INSTRUCTION_SYNC:
        if (item->_direction != SyncFileItem::Up) {
            auto job = new PropagateDownloadFile(this, item);
            job->setDeleteExistingFolder(deleteExisting);
            return job;
        } else {
            PropagateUploadFileCommon *job = nullptr;
            if (account()->capabilities().tusSupport().isValid()) {
                job = new PropagateUploadFileTUS(this, item);
            } else {
                if (item->_size > syncOptions()._initialChunkSize && account()->capabilities().chunkingNg()) {
                    // Item is above _initialChunkSize, thus will be classified as to be chunked
                    job = new PropagateUploadFileNG(this, item);
                } else {
                    job = new PropagateUploadFileV1(this, item);
                }
            }
            job->setDeleteExisting(deleteExisting);
            return job;
        }
    case CSYNC_INSTRUCTION_RENAME:
        if (item->_direction == SyncFileItem::Up) {
            return new PropagateRemoteMove(this, item);
        } else {
            return new PropagateLocalRename(this, item);
        }
    case CSYNC_INSTRUCTION_UPDATE_METADATA:
        // For directories, metadata-only updates will be done after all their files are propagated.
        if (item->isDirectory()) {
            return nullptr;
        }
        return new PropagateUpdateMetaDataJob(this, item);
    case CSYNC_INSTRUCTION_IGNORE:
    case CSYNC_INSTRUCTION_ERROR:
        return new PropagateIgnoreJob(this, item);
    case CSYNC_INSTRUCTION_NONE:
    case CSYNC_INSTRUCTION_STAT_ERROR:
    case CSYNC_INSTRUCTION_EVAL_RENAME:
    case CSYNC_INSTRUCTION_EVAL:
        return nullptr;
    }
    Q_UNREACHABLE();
}

qint64 OwncloudPropagator::smallFileSize()
{
    const qint64 smallFileSize = 100 * 1024; //default to 1 MB. Not dynamic right now.
    return smallFileSize;
}

void OwncloudPropagator::start(SyncFileItemSet &&items)
{
    /* This builds all the jobs needed for the propagation.
     * Each directory is a PropagateDirectory job, which contains the files in it.
     * In order to do that we loop over the items. (which are sorted by destination)
     * When we enter a directory, we can create the directory job and push it on the stack. */

    _rootJob.reset(new PropagateRootDirectory(this));
    QStack<QPair<QString /* directory name */, PropagateDirectory * /* job */>> directories;
    directories.push(qMakePair(QString(), _rootJob.data()));
    QVector<PropagatorJob *> directoriesToRemove;
    QString removedDirectory;
    QString maybeConflictDirectory;
    for (const auto &item : qAsConst(items)) {
        if (!removedDirectory.isEmpty() && item->_file.startsWith(removedDirectory)) {
            // this is an item in a directory which is going to be removed.
            PropagateDirectory *delDirJob = qobject_cast<PropagateDirectory *>(directoriesToRemove.first());

            if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                // already taken care of. (by the removal of the parent directory)

                // increase the number of subjobs that would be there.
                if (delDirJob) {
                    delDirJob->increaseAffectedCount();
                }
                continue;
            } else if (item->isDirectory()
                && (item->_instruction == CSYNC_INSTRUCTION_NEW
                       || item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE)) {
                // create a new directory within a deleted directory? That can happen if the directory
                // etag was not fetched properly on the previous sync because the sync was aborted
                // while uploading this directory (which is now removed).  We can ignore it.
                if (delDirJob) {
                    delDirJob->increaseAffectedCount();
                }
                continue;
            } else if (item->_instruction == CSYNC_INSTRUCTION_IGNORE) {
                continue;
            } else if (item->_instruction == CSYNC_INSTRUCTION_RENAME) {
                // all is good, the rename will be executed before the directory deletion
            } else {
                qCWarning(lcPropagator) << "WARNING:  Job within a removed directory?  This should not happen!"
                                        << item->_file << item->_instruction;
            }
        }

        // If a CONFLICT item contains files these can't be processed because
        // the conflict handling is likely to rename the directory. This can happen
        // when there's a new local directory at the same time as a remote file.
        if (!maybeConflictDirectory.isEmpty()) {
            if (item->destination().startsWith(maybeConflictDirectory)) {
                qCInfo(lcPropagator) << "Skipping job inside CONFLICT directory"
                                     << item->_file << item->_instruction;
                item->_instruction = CSYNC_INSTRUCTION_NONE;
                continue;
            } else {
                maybeConflictDirectory.clear();
            }
        }

        while (!item->destination().startsWith(directories.top().first)) {
            directories.pop();
        }

        if (item->isDirectory()) {
            PropagateDirectory *dir = new PropagateDirectory(this, item);

            if (item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE
                && item->_direction == SyncFileItem::Up) {
                // Skip all potential uploads to the new folder.
                // Processing them now leads to problems with permissions:
                // checkForPermissions() has already run and used the permissions
                // of the file we're about to delete to decide whether uploading
                // to the new dir is ok...
                for (const auto &item2 : qAsConst(items)) {
                    if (item2->destination().startsWith(item->destination() + QLatin1Char('/'))) {
                        item2->_instruction = CSYNC_INSTRUCTION_NONE;
                        _anotherSyncNeeded = true;
                    }
                }
            }

            if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                // We do the removal of directories at the end, because there might be moves from
                // these directories that will happen later.
                directoriesToRemove.prepend(dir);
                removedDirectory = item->_file + QLatin1Char('/');

                // We should not update the etag of parent directories of the removed directory
                // since it would be done before the actual remove (issue #1845)
                // NOTE: Currently this means that we don't update those etag at all in this sync,
                //       but it should not be a problem, they will be updated in the next sync.
                for (int i = 0; i < directories.size(); ++i) {
                    if (directories[i].second->_item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA)
                        directories[i].second->_item->_instruction = CSYNC_INSTRUCTION_NONE;
                }
            } else {
                PropagateDirectory *currentDirJob = directories.top().second;
                currentDirJob->appendJob(dir);
            }
            directories.push(qMakePair(item->destination() + QLatin1Char('/'), dir));
        } else {
            if (item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE) {
                // will delete directories, so defer execution
                directoriesToRemove.prepend(createJob(item));
                removedDirectory = item->_file + QLatin1Char('/');
            } else {
                directories.top().second->appendTask(item);
            }

            if (item->_instruction == CSYNC_INSTRUCTION_CONFLICT) {
                // This might be a file or a directory on the local side. If it's a
                // directory we want to skip processing items inside it.
                maybeConflictDirectory = item->_file + QLatin1Char('/');
            }
        }
    }

    for (auto *it : qAsConst(directoriesToRemove)) {
        _rootJob->_dirDeletionJobs.appendJob(it);
    }

    connect(_rootJob.data(), &PropagatorJob::finished, this, &OwncloudPropagator::emitFinished);

    _jobScheduled = false;
    scheduleNextJob();
}

const SyncOptions &OwncloudPropagator::syncOptions() const
{
    return _syncOptions;
}

void OwncloudPropagator::setSyncOptions(const SyncOptions &syncOptions)
{
    _syncOptions = syncOptions;
    _chunkSize = syncOptions._initialChunkSize;
}

Result<QString, bool> OwncloudPropagator::localFileNameClash(const QString &relFile)
{
    OC_ASSERT(!relFile.isEmpty());
    if (!relFile.isEmpty() && Utility::fsCasePreserving()) {
        const QFileInfo fileInfo(_localDir + relFile);
        qCDebug(lcPropagator) << "CaseClashCheck for " << fileInfo.filePath();
#ifdef Q_OS_MAC
        if (!fileInfo.exists()) {
            return false;
        } else {
            // Need to normalize to composited form because of QTBUG-39622/QTBUG-55896
            const QString cName = fileInfo.canonicalFilePath().normalized(QString::NormalizationForm_C);
            if (fileInfo.filePath() != cName && !cName.endsWith(relFile, Qt::CaseSensitive)) {
                qCWarning(lcPropagator) << "Detected case clash between" << fileInfo.filePath() << "and" << cName;
                return cName;
            }
        }
#elif defined(Q_OS_WIN)
        WIN32_FIND_DATA FindFileData;
        HANDLE hFind;

        hFind = FindFirstFileW(reinterpret_cast<const wchar_t *>(FileSystem::longWinPath(fileInfo.filePath()).utf16()), &FindFileData);
        if (hFind != INVALID_HANDLE_VALUE) {
            const QString realFileName = QString::fromWCharArray(FindFileData.cFileName);
            FindClose(hFind);

            if (!fileInfo.filePath().endsWith(realFileName, Qt::CaseSensitive)) {
                const QString clashName = fileInfo.path() + QLatin1Char('/') + realFileName;
                qCWarning(lcPropagator) << "Detected case clash between" << fileInfo.filePath() << "and" << clashName;
                return clashName;
            }
        }
#else
        // On Linux, the file system is case sensitive, but this code is useful for testing.
        // Just check that there is no other file with the same name and different casing.
        const QString fn = fileInfo.fileName();
        const QStringList list = fileInfo.dir().entryList({ fn });
        if (list.count() > 1 || (list.count() == 1 && list[0] != fn)) {
            return list[0];
        }
#endif
    }
    return false;
}

bool OwncloudPropagator::hasCaseClashAccessibilityProblem(const QString &relfile)
{
#ifdef Q_OS_WIN
    bool result = false;
    const QString file(_localDir + relfile);
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

    hFind = FindFirstFileW(reinterpret_cast<const wchar_t *>(FileSystem::longWinPath(file).utf16()), &FindFileData);
    if (hFind != INVALID_HANDLE_VALUE) {
        QString firstFile = QString::fromWCharArray(FindFileData.cFileName);
        if (FindNextFile(hFind, &FindFileData)) {
            QString secondFile = QString::fromWCharArray(FindFileData.cFileName);
            // This extra check shouldn't be necessary, but ensures that there
            // are two different filenames that are identical when case is ignored.
            if (firstFile != secondFile
                && QString::compare(firstFile, secondFile, Qt::CaseInsensitive) == 0) {
                result = true;
                qCWarning(lcPropagator) << "Found two filepaths that only differ in case: " << firstFile << secondFile;
            }
        }
        FindClose(hFind);
    }
    return result;
#else
    Q_UNUSED(relfile);
    return false;
#endif
}

QString OwncloudPropagator::fullLocalPath(const QString &tmp_file_name) const
{
    return _localDir + tmp_file_name;
}

QString OwncloudPropagator::localPath() const
{
    return _localDir;
}

void OwncloudPropagator::scheduleNextJob()
{
    if (_jobScheduled) return; // don't schedule more than 1
    _jobScheduled = true;
    QTimer::singleShot(3, this, &OwncloudPropagator::scheduleNextJobImpl);
}

void OwncloudPropagator::scheduleNextJobImpl()
{
    // TODO: If we see that the automatic up-scaling has a bad impact we
    // need to check how to avoid this.
    // Down-scaling on slow networks? https://github.com/owncloud/client/issues/3382
    // Making sure we do up/down at same time? https://github.com/owncloud/client/issues/1633

    _jobScheduled = false;

    if (_activeJobList.count() < maximumActiveTransferJob()) {
        if (_rootJob->scheduleSelfOrChild()) {
            scheduleNextJob();
        }
    } else if (_activeJobList.count() < hardMaximumActiveJob()) {
        int likelyFinishedQuicklyCount = 0;
        // NOTE: Only counts the first 3 jobs! Then for each
        // one that is likely finished quickly, we can launch another one.
        // When a job finishes another one will "move up" to be one of the first 3 and then
        // be counted too.
        for (int i = 0; i < maximumActiveTransferJob() && i < _activeJobList.count(); i++) {
            if (_activeJobList.at(i)->isLikelyFinishedQuickly()) {
                likelyFinishedQuicklyCount++;
            }
        }
        if (_activeJobList.count() < maximumActiveTransferJob() + likelyFinishedQuicklyCount) {
            qCDebug(lcPropagator) << "Can pump in another request! activeJobs =" << _activeJobList.count();
            if (_rootJob->scheduleSelfOrChild()) {
                scheduleNextJob();
            }
        }
    }
}

void OwncloudPropagator::reportFileTotal(const SyncFileItem &item, qint64 newSize)
{
    emit updateFileTotal(item, newSize);
}

void OwncloudPropagator::reportProgress(const SyncFileItem &item, qint64 bytes)
{
    emit progress(item, bytes);
}

AccountPtr OwncloudPropagator::account() const
{
    return _account;
}

OwncloudPropagator::DiskSpaceResult OwncloudPropagator::diskSpaceCheck() const
{
    const qint64 freeBytes = Utility::freeDiskSpace(_localDir);
    if (freeBytes < 0) {
        return DiskSpaceOk;
    }

    if (freeBytes < criticalFreeSpaceLimit()) {
        return DiskSpaceCritical;
    }

    if (freeBytes - _rootJob->committedDiskSpace() < freeSpaceLimit()) {
        return DiskSpaceFailure;
    }

    return DiskSpaceOk;
}

bool OwncloudPropagator::createConflict(const SyncFileItemPtr &item,
    PropagatorCompositeJob *composite, QString *error)
{
    QString fn = fullLocalPath(item->_file);

    QString renameError;
    auto conflictModTime = FileSystem::getModTime(fn);
    QString conflictUserName;
    if (account()->capabilities().uploadConflictFiles())
        conflictUserName = account()->davDisplayName();
    QString conflictFileName = Utility::makeConflictFileName(
        item->_file, Utility::qDateTimeFromTime_t(conflictModTime), conflictUserName);
    QString conflictFilePath = fullLocalPath(conflictFileName);

    // If the file is locked, we want to retry this sync when it
    // becomes available again.
    if (FileSystem::isFileLocked(fn, FileSystem::LockMode::Exclusive)) {
        emit seenLockedFile(fn, FileSystem::LockMode::Exclusive);
        if (error)
            *error = tr("File %1 is currently in use").arg(fn);
        return false;
    }

    emit touchedFile(fn);
    emit touchedFile(conflictFilePath);
    if (!FileSystem::rename(fn, conflictFilePath, &renameError)) {
        // If the rename fails, don't replace it.
        if (error)
            *error = renameError;
        return false;
    }
    qCInfo(lcPropagator) << "Created conflict file" << fn << "->" << conflictFileName;

    // Create a new conflict record. To get the base etag, we need to read it from the db.
    ConflictRecord conflictRecord;
    conflictRecord.path = conflictFileName.toUtf8();
    conflictRecord.baseModtime = item->_previousModtime;
    conflictRecord.initialBasePath = item->_file.toUtf8();

    SyncJournalFileRecord baseRecord;
    if (_journal->getFileRecord(item->_originalFile, &baseRecord) && baseRecord.isValid()) {
        conflictRecord.baseEtag = baseRecord._etag;
        conflictRecord.baseFileId = baseRecord._fileId;
    } else {
        // We might very well end up with no fileid/etag for new/new conflicts
    }

    _journal->setConflictRecord(conflictRecord);

    // Create a new upload job if the new conflict file should be uploaded
    if (account()->capabilities().uploadConflictFiles()) {
        if (composite && !QFileInfo(conflictFilePath).isDir()) {
            SyncFileItemPtr conflictItem = SyncFileItemPtr(new SyncFileItem);
            conflictItem->_file = conflictFileName;
            conflictItem->_type = ItemTypeFile;
            conflictItem->_direction = SyncFileItem::Up;
            conflictItem->_instruction = CSYNC_INSTRUCTION_NEW;
            conflictItem->_modtime = conflictModTime;
            conflictItem->_size = item->_previousSize;
            emit newItem(conflictItem);
            composite->appendTask(conflictItem);
        } else {
            // Directories we can't process in one go. The next sync run
            // will take care of uploading the conflict dir contents.
            _anotherSyncNeeded = true;
        }
    }

    return true;
}

QString OwncloudPropagator::adjustRenamedPath(const QString &original) const
{
    return OCC::adjustRenamedPath(_renamedDirectories, original);
}

Result<Vfs::ConvertToPlaceholderResult, QString> OwncloudPropagator::updatePlaceholder(const SyncFileItem &item, const QString &fileName, const QString &replacesFile)
{
    const auto result = syncOptions()._vfs->updateMetadata(item, fileName, replacesFile);
    if (!result) {
        return result.error();
    } else if (*result == Vfs::ConvertToPlaceholderResult::Locked) {
        return Vfs::ConvertToPlaceholderResult::Locked;
    }
    return Vfs::ConvertToPlaceholderResult::Ok;
}

Result<Vfs::ConvertToPlaceholderResult, QString> OwncloudPropagator::updateMetadata(const SyncFileItem &item)
{
    const QString fsPath = fullLocalPath(item.destination());
    const auto result = updatePlaceholder(item, fsPath, {});
    if (!result) {
        return result;
    }
    const auto record = item.toSyncJournalFileRecordWithInode(fsPath);
    const auto dBresult = _journal->setFileRecord(record);
    if (!dBresult) {
        return dBresult.error();
    }
    return Vfs::ConvertToPlaceholderResult::Ok;
}

// ================================================================================

PropagatorJob::PropagatorJob(OwncloudPropagator *propagator)
    : QObject(propagator)
    , _state(NotYetStarted)
{
}

OwncloudPropagator *PropagatorJob::propagator() const
{
    return qobject_cast<OwncloudPropagator *>(parent());
}

// ================================================================================

PropagatorJob::JobParallelism PropagatorCompositeJob::parallelism()
{
    // If any of the running sub jobs is not parallel, we have to wait
    for (int i = 0; i < _runningJobs.count(); ++i) {
        if (_runningJobs.at(i)->parallelism() != FullParallelism) {
            return _runningJobs.at(i)->parallelism();
        }
    }
    return FullParallelism;
}

void PropagatorCompositeJob::slotSubJobAbortFinished()
{
    // Count that job has been finished
    _abortsCount--;

    // Emit abort if last job has been aborted
    if (_abortsCount == 0) {
        emit abortFinished();
    }
}

void PropagatorCompositeJob::appendJob(PropagatorJob *job)
{
    job->setAssociatedComposite(this);
    _jobsToDo.append(job);
}

bool PropagatorCompositeJob::scheduleSelfOrChild()
{
    if (_state == Finished) {
        return false;
    }

    // Start the composite job
    if (_state == NotYetStarted) {
        _state = Running;
    }

    // Ask all the running composite jobs if they have something new to schedule.
    for (int i = 0; i < _runningJobs.size(); ++i) {
        OC_ASSERT(_runningJobs.at(i)->_state == Running);

        if (possiblyRunNextJob(_runningJobs.at(i))) {
            return true;
        }

        // If any of the running sub jobs is not parallel, we have to cancel the scheduling
        // of the rest of the list and wait for the blocking job to finish and schedule the next one.
        auto paral = _runningJobs.at(i)->parallelism();
        if (paral == WaitForFinished) {
            return false;
        }
    }

    // Now it's our turn, check if we have something left to do.
    // First, convert a task to a job if necessary
    while (_jobsToDo.empty() && !_tasksToDo.empty()) {
        const SyncFileItemPtr nextTask = *_tasksToDo.begin();
        _tasksToDo.erase(_tasksToDo.begin());
        PropagatorJob *job = propagator()->createJob(nextTask);
        if (!job) {
            qCWarning(lcDirectory) << "Useless task found for file" << nextTask->destination() << "instruction" << nextTask->_instruction;
            continue;
        }
        appendJob(job);
        break;
    }
    // Then run the next job
    if (!_jobsToDo.isEmpty()) {
        PropagatorJob *nextJob = _jobsToDo.first();
        _jobsToDo.remove(0);
        _runningJobs.append(nextJob);
        return possiblyRunNextJob(nextJob);
    }

    // If neither us or our children had stuff left to do we could hang. Make sure
    // we mark this job as finished so that the propagator can schedule a new one.
    if (_jobsToDo.isEmpty() && _tasksToDo.empty() && _runningJobs.isEmpty()) {
        // Our parent jobs are already iterating over their running jobs, post to the event loop
        // to avoid removing ourself from that list while they iterate.
        QMetaObject::invokeMethod(this, "finalize", Qt::QueuedConnection);
    }
    return false;
}

void PropagatorCompositeJob::slotSubJobFinished(SyncFileItem::Status status)
{
    PropagatorJob *subJob = static_cast<PropagatorJob *>(sender());
    OC_ASSERT(subJob);

    // Delete the job and remove it from our list of jobs.
    subJob->deleteLater();
    int i = _runningJobs.indexOf(subJob);
    OC_ENFORCE(i >= 0); // should only happen if this function is called more than once
    _runningJobs.remove(i);

    // Any sub job error will cause the whole composite to fail. This is important
    // for knowing whether to update the etag in PropagateDirectory, for example.
    if (status == SyncFileItem::FatalError
        || status == SyncFileItem::NormalError
        || status == SyncFileItem::SoftError
        || status == SyncFileItem::DetailError
        || status == SyncFileItem::BlacklistedError) {
        _hasError = status;
    }

    if (_jobsToDo.isEmpty() && _tasksToDo.empty() && _runningJobs.isEmpty()) {
        finalize();
    } else {
        propagator()->scheduleNextJob();
    }
}

void PropagatorCompositeJob::finalize()
{
    // The propagator will do parallel scheduling and this could be posted
    // multiple times on the event loop, ignore the duplicate calls.
    if (_state == Finished)
        return;

    _state = Finished;
    emit finished(_hasError == SyncFileItem::NoStatus ? SyncFileItem::Success : _hasError);
}

qint64 PropagatorCompositeJob::committedDiskSpace() const
{
    qint64 needed = 0;
    for (auto *job : _runningJobs) {
        needed += job->committedDiskSpace();
    }
    return needed;
}

// ================================================================================

PropagateDirectory::PropagateDirectory(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
    : PropagatorJob(propagator)
    , _item(item)
    , _firstJob(propagator->createJob(item))
    , _subJobs(propagator)
{
    if (_firstJob) {
        connect(_firstJob.data(), &PropagatorJob::finished, this, &PropagateDirectory::slotFirstJobFinished);
        _firstJob->setAssociatedComposite(&_subJobs);
    }
    connect(&_subJobs, &PropagatorJob::finished, this, &PropagateDirectory::slotSubJobsFinished);
}

PropagatorJob::JobParallelism PropagateDirectory::parallelism()
{
    // If any of the non-finished sub jobs is not parallel, we have to wait
    if (_firstJob && _firstJob->parallelism() != FullParallelism) {
        return WaitForFinished;
    }
    if (_subJobs.parallelism() != FullParallelism) {
        return WaitForFinished;
    }
    return FullParallelism;
}


bool PropagateDirectory::scheduleSelfOrChild()
{
    if (_state == Finished) {
        return false;
    }

    if (_state == NotYetStarted) {
        _state = Running;
    }

    if (_firstJob && _firstJob->_state == NotYetStarted) {
        return _firstJob->scheduleSelfOrChild();
    }

    if (_firstJob && _firstJob->_state == Running) {
        // Don't schedule any more job until this is done.
        return false;
    }

    return _subJobs.scheduleSelfOrChild();
}

void PropagateDirectory::slotFirstJobFinished(SyncFileItem::Status status)
{
    _firstJob.take()->deleteLater();

    if (status != SyncFileItem::Success
        && status != SyncFileItem::Restoration
        && status != SyncFileItem::Conflict) {
        if (_state != Finished) {
            // Synchronously abort
            abort(AbortType::Synchronous);
            _state = Finished;
            emit finished(status);
        }
        return;
    }

    propagator()->scheduleNextJob();
}

void PropagateDirectory::slotSubJobsFinished(SyncFileItem::Status status)
{
    if (!_item->isEmpty() && status == SyncFileItem::Success) {
        // If a directory is renamed, recursively delete any stale items
        // that may still exist below the old path.
        if (_item->_instruction == CSYNC_INSTRUCTION_RENAME
            && _item->_originalFile != _item->_renameTarget) {
            propagator()->_journal->deleteFileRecord(_item->_originalFile, true);
        }

        if (_item->_instruction == CSYNC_INSTRUCTION_NEW && _item->_direction == SyncFileItem::Down) {
            // special case for local MKDIR, set local directory mtime
            // (it's not synced later at all, but can be nice to have it set initially)
            FileSystem::setModTime(propagator()->fullLocalPath(_item->destination()), _item->_modtime);
        }

        // For new directories we always want to update the etag once
        // the directory has been propagated. Otherwise the directory
        // could appear locally without being added to the database.
        if (_item->_instruction == CSYNC_INSTRUCTION_RENAME
            || _item->_instruction == CSYNC_INSTRUCTION_NEW
            || _item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA) {
            const auto result = propagator()->updateMetadata(*_item);
            if (!result) {
                status = _item->_status = SyncFileItem::FatalError;
                _item->_errorString = tr("Error updating metadata: %1").arg(result.error());
                qCWarning(lcDirectory) << "Error writing to the database for file" << _item->_file << "with" << result.error();
            } else if (result.get() == Vfs::ConvertToPlaceholderResult::Locked) {
                _item->_status = SyncFileItem::SoftError;
                _item->_errorString = tr("File is currently in use");
            }
        }
    }
    _state = Finished;
    emit finished(status);
}

PropagateRootDirectory::PropagateRootDirectory(OwncloudPropagator *propagator)
    : PropagateDirectory(propagator, SyncFileItemPtr(new SyncFileItem))
    , _dirDeletionJobs(propagator)
{
    connect(&_dirDeletionJobs, &PropagatorJob::finished, this, &PropagateRootDirectory::slotDirDeletionJobsFinished);
}

PropagatorJob::JobParallelism PropagateRootDirectory::parallelism()
{
    // the root directory parallelism isn't important
    return WaitForFinished;
}

void PropagateRootDirectory::abort(PropagatorJob::AbortType abortType)
{
    if (_firstJob)
        // Force first job to abort synchronously
        // even if caller allows async abort (asyncAbort)
        _firstJob->abort(AbortType::Synchronous);

    if (abortType == AbortType::Asynchronous) {
        struct AbortsFinished {
            bool subJobsFinished = false;
            bool dirDeletionFinished = false;
        };
        auto abortStatus = QSharedPointer<AbortsFinished>(new AbortsFinished);

        connect(&_subJobs, &PropagatorCompositeJob::abortFinished, this, [this, abortStatus]() {
            abortStatus->subJobsFinished = true;
            if (abortStatus->subJobsFinished && abortStatus->dirDeletionFinished)
                emit abortFinished();
        });
        connect(&_dirDeletionJobs, &PropagatorCompositeJob::abortFinished, this, [this, abortStatus]() {
            abortStatus->dirDeletionFinished = true;
            if (abortStatus->subJobsFinished && abortStatus->dirDeletionFinished)
                emit abortFinished();
        });
    }
    _subJobs.abort(abortType);
    _dirDeletionJobs.abort(abortType);
}

qint64 PropagateRootDirectory::committedDiskSpace() const
{
    return _subJobs.committedDiskSpace() + _dirDeletionJobs.committedDiskSpace();
}

bool PropagateRootDirectory::scheduleSelfOrChild()
{
    if (_state == Finished)
        return false;

    if (PropagateDirectory::scheduleSelfOrChild())
        return true;

    // Important: Finish _subJobs before scheduling any deletes.
    if (_subJobs._state != Finished)
        return false;

    return _dirDeletionJobs.scheduleSelfOrChild();
}

void PropagateRootDirectory::slotSubJobsFinished(SyncFileItem::Status status)
{
    if (status != SyncFileItem::Success
        && status != SyncFileItem::Restoration
        && status != SyncFileItem::Conflict) {
        if (_state != Finished) {
            // Synchronously abort
            abort(AbortType::Synchronous);
            _state = Finished;
            emit finished(status);
        }
        return;
    }

    propagator()->scheduleNextJob();
}

void PropagateRootDirectory::slotDirDeletionJobsFinished(SyncFileItem::Status status)
{
    _state = Finished;
    emit finished(status);
}

// ================================================================================

QString OwncloudPropagator::fullRemotePath(const QString &tmp_file_name) const
{
    // TODO: should this be part of the _item (SyncFileItemPtr)?
    return _remoteFolder + tmp_file_name;
}

QString OwncloudPropagator::remotePath() const
{
    return _remoteFolder;
}

PropagateUpdateMetaDataJob::PropagateUpdateMetaDataJob(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
    : PropagateItemJob(propagator, item)
{
}

void OCC::PropagateUpdateMetaDataJob::start()
{
    // Update the database:  New remote fileid or Etag or RemotePerm
    // Or for files that were detected as "resolved conflict".
    // Or a local inode/mtime change

    // In case of "resolved conflict": there should have been a conflict because they
    // both were new, or both had their local mtime or remote etag modified, but the
    // size and mtime is the same on the server.  This typically happens when the
    // database is removed. Nothing will be done for those files, but we still need
    // to update the database.

    const QString filePath = propagator()->fullLocalPath(_item->destination());
    if (_item->_direction == SyncFileItem::Down) {
        // If the 'W' remote permission changed, update the local filesystem
        SyncJournalFileRecord prev;
        if (propagator()->_journal->getFileRecord(_item->_file, &prev)
            && prev.isValid()
            && prev._remotePerm.hasPermission(RemotePermissions::CanWrite) != _item->_remotePerm.hasPermission(RemotePermissions::CanWrite)) {
            const bool isReadOnly = !_item->_remotePerm.isNull() && !_item->_remotePerm.hasPermission(RemotePermissions::CanWrite);
            FileSystem::setFileReadOnlyWeak(filePath, isReadOnly);
            if (_item->_checksumHeader.isEmpty()) {
                _item->_checksumHeader = prev._checksumHeader;
            }
            _item->_serverHasIgnoredFiles |= prev._serverHasIgnoredFiles;
        }
    }
    const auto result = propagator()->updateMetadata(*_item);
    if (!result) {
        done(SyncFileItem::SoftError, tr("Could not update file : %1").arg(result.error()));
    }
    done(SyncFileItem::Success);
}
}

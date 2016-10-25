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
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include "propagatedownload.h"
#include "propagateupload.h"
#include "propagateremotedelete.h"
#include "propagateremotemove.h"
#include "propagateremotemkdir.h"
#include "propagatorjobs.h"
#include "configfile.h"
#include "utility.h"
#include "account.h"
#include <json.h>

#ifdef Q_OS_WIN
#include <windef.h>
#include <winbase.h>
#endif

#include <QStack>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QObject>
#include <QTimerEvent>
#include <QDebug>

namespace OCC {

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
{}

/* The maximum number of active jobs in parallel  */
int OwncloudPropagator::maximumActiveJob()
{
    static int max = qgetenv("OWNCLOUD_MAX_PARALLEL").toUInt();
    if (!max) {
        max = 3; //default
    }

    if (_downloadLimit.fetchAndAddAcquire(0) != 0 || _uploadLimit.fetchAndAddAcquire(0) != 0) {
        // disable parallelism when there is a network limit.
        return 1;
    }

    return max;
}

int OwncloudPropagator::hardMaximumActiveJob()
{
    int max = maximumActiveJob();
    return max*2;
    // FIXME: Wondering if we should hard-limit to 1 if maximumActiveJob() is 1
    // to support our old use case of limiting concurrency (when "automatic" bandwidth
    // limiting is set. But this causes https://github.com/owncloud/client/issues/4081
}


/** Updates, creates or removes a blacklist entry for the given item.
 *
 * Returns whether the file is in the blacklist now.
 */
static bool blacklistCheck(SyncJournalDb* journal, const SyncFileItem& item)
{
    SyncJournalErrorBlacklistRecord oldEntry = journal->errorBlacklistEntry(item._file);
    SyncJournalErrorBlacklistRecord newEntry = SyncJournalErrorBlacklistRecord::update(oldEntry, item);

    if (newEntry.isValid()) {
        journal->updateErrorBlacklistEntry(newEntry);
    } else if (oldEntry.isValid()) {
        journal->wipeErrorBlacklistEntry(item._file);
    }

    return newEntry.isValid();
}

void PropagateItemJob::done(SyncFileItem::Status status, const QString &errorString)
{
    _state = Finished;
    if (_item->_isRestoration) {
        if( status == SyncFileItem::Success || status == SyncFileItem::Conflict) {
            status = SyncFileItem::Restoration;
        } else {
            _item->_errorString += tr("; Restoration Failed: %1").arg(errorString);
        }
    } else {
        if( _item->_errorString.isEmpty() ) {
            _item->_errorString = errorString;
        }
    }

    if( _propagator->_abortRequested.fetchAndAddRelaxed(0) &&
            (status == SyncFileItem::NormalError || status == SyncFileItem::FatalError)) {
        // an abort request is ongoing. Change the status to Soft-Error
        status = SyncFileItem::SoftError;
    }

    switch( status ) {
    case SyncFileItem::SoftError:
    case SyncFileItem::FatalError:
        // do not blacklist in case of soft error or fatal error.
        break;
    case SyncFileItem::NormalError:
        if (blacklistCheck(_propagator->_journal, *_item) && _item->_hasBlacklistEntry) {
            // do not error if the item was, and continues to be, blacklisted
            status = SyncFileItem::FileIgnored;
            _item->_errorString.prepend(tr("Continue blacklisting:") + " ");
        }
        break;
    case SyncFileItem::Success:
    case SyncFileItem::Restoration:
        if( _item->_hasBlacklistEntry ) {
            // wipe blacklist entry.
            _propagator->_journal->wipeErrorBlacklistEntry(_item->_file);
            // remove a blacklist entry in case the file was moved.
            if( _item->_originalFile != _item->_file ) {
                _propagator->_journal->wipeErrorBlacklistEntry(_item->_originalFile);
            }
        }
        break;
    case SyncFileItem::Conflict:
    case SyncFileItem::FileIgnored:
    case SyncFileItem::NoStatus:
        // nothing
        break;
    }

    _item->_status = status;

    emit itemCompleted(*_item, *this);
    emit finished(status);
}

/**
 * For delete or remove, check that we are not removing from a shared directory.
 * If we are, try to restore the file
 *
 * Return true if the problem is handled.
 */
bool PropagateItemJob::checkForProblemsWithShared(int httpStatusCode, const QString& msg)
{
    PropagateItemJob *newJob = NULL;

    if( httpStatusCode == 403 && _propagator->isInSharedDirectory(_item->_file )) {
        if( !_item->_isDirectory ) {
            SyncFileItemPtr downloadItem(new SyncFileItem(*_item));
            if (downloadItem->_instruction == CSYNC_INSTRUCTION_NEW
                    || downloadItem->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE) {
                // don't try to recover pushing new files
                return false;
            } else if (downloadItem->_instruction == CSYNC_INSTRUCTION_SYNC) {
                // we modified the file locally, just create a conflict then
                downloadItem->_instruction = CSYNC_INSTRUCTION_CONFLICT;

                // HACK to avoid continuation: See task #1448:  We do not know the _modtime from the
                //  server, at this point, so just set the current one. (rather than the one locally)
                downloadItem->_modtime = Utility::qDateTimeToTime_t(QDateTime::currentDateTime());
            } else {
                // the file was removed or renamed, just recover the old one
                downloadItem->_instruction = CSYNC_INSTRUCTION_SYNC;
            }
            downloadItem->_direction = SyncFileItem::Down;
            newJob = new PropagateDownloadFile(_propagator, downloadItem);
        } else {
            // Directories are harder to recover.
            // But just re-create the directory, next sync will be able to recover the files
            SyncFileItemPtr mkdirItem(new SyncFileItem(*_item));
            mkdirItem->_instruction = CSYNC_INSTRUCTION_NEW;
            mkdirItem->_direction = SyncFileItem::Down;
            newJob = new PropagateLocalMkdir(_propagator, mkdirItem);
            // Also remove the inodes and fileid from the db so no further renames are tried for
            // this item.
            _propagator->_journal->avoidRenamesOnNextSync(_item->_file);
            _propagator->_anotherSyncNeeded = true;
        }
        if( newJob )  {
            newJob->setRestoreJobMsg(msg);
            _restoreJob.reset(newJob);
            connect(_restoreJob.data(), SIGNAL(itemCompleted(const SyncFileItemPtr &, const PropagatorJob &)),
                    this, SLOT(slotRestoreJobCompleted(const SyncFileItemPtr &)));
            QMetaObject::invokeMethod(newJob, "start");
        }
        return true;
    }
    return false;
}

void PropagateItemJob::slotRestoreJobCompleted(const SyncFileItem& item )
{
    QString msg;
    if(_restoreJob) {
        msg = _restoreJob->restoreJobMsg();
        _restoreJob->setRestoreJobMsg();
    }

    if( item._status == SyncFileItem::Success ||  item._status == SyncFileItem::Conflict
            || item._status == SyncFileItem::Restoration) {
        done( SyncFileItem::SoftError, msg);
    } else {
        done( item._status, tr("A file or folder was removed from a read only share, but restoring failed: %1").arg(item._errorString) );
    }
}

// ================================================================================

PropagateItemJob* OwncloudPropagator::createJob(const SyncFileItemPtr &item) {
    bool deleteExisting = item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE;
    switch(item->_instruction) {
        case CSYNC_INSTRUCTION_REMOVE:
            if (item->_direction == SyncFileItem::Down) return new PropagateLocalRemove(this, item);
            else return new PropagateRemoteDelete(this, item);
        case CSYNC_INSTRUCTION_NEW:
        case CSYNC_INSTRUCTION_TYPE_CHANGE:
            if (item->_isDirectory) {
                if (item->_direction == SyncFileItem::Down) {
                    auto job = new PropagateLocalMkdir(this, item);
                    job->setDeleteExistingFile(deleteExisting);
                    return job;
                } else {
                    auto job = new PropagateRemoteMkdir(this, item);
                    job->setDeleteExisting(deleteExisting);
                    return job;
                }
            }   //fall through
        case CSYNC_INSTRUCTION_SYNC:
        case CSYNC_INSTRUCTION_CONFLICT:
            if (item->_direction != SyncFileItem::Up) {
                auto job = new PropagateDownloadFile(this, item);
                job->setDeleteExistingFolder(deleteExisting);
                return job;
            } else {
                auto job = new PropagateUploadFile(this, item);
                job->setDeleteExisting(deleteExisting);
                return job;
            }
        case CSYNC_INSTRUCTION_RENAME:
            if (item->_direction == SyncFileItem::Up) {
                return new PropagateRemoteMove(this, item);
            } else {
                return new PropagateLocalRename(this, item);
            }
        case CSYNC_INSTRUCTION_IGNORE:
        case CSYNC_INSTRUCTION_ERROR:
            return new PropagateIgnoreJob(this, item);
        default:
            return 0;
    }
    return 0;
}

void OwncloudPropagator::start(const SyncFileItemVector& items)
{
    Q_ASSERT(std::is_sorted(items.begin(), items.end()));

    /* This builds all the jobs needed for the propagation.
     * Each directory is a PropagateDirectory job, which contains the files in it.
     * In order to do that we loop over the items. (which are sorted by destination)
     * When we enter a directory, we can create the directory job and push it on the stack. */

    _rootJob.reset(new PropagateDirectory(this));
    QStack<QPair<QString /* directory name */, PropagateDirectory* /* job */> > directories;
    directories.push(qMakePair(QString(), _rootJob.data()));
    QVector<PropagatorJob*> directoriesToRemove;
    QString removedDirectory;
    foreach(const SyncFileItemPtr &item, items) {

        if (!removedDirectory.isEmpty() && item->_file.startsWith(removedDirectory)) {
            // this is an item in a directory which is going to be removed.
            PropagateDirectory *delDirJob = dynamic_cast<PropagateDirectory*>(directoriesToRemove.first());

            if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                // already taken care of. (by the removal of the parent directory)

                // increase the number of subjobs that would be there.
                if( delDirJob ) {
                    delDirJob->increaseAffectedCount();
                }
                continue;
            } else if (item->_isDirectory
                       && (item->_instruction == CSYNC_INSTRUCTION_NEW
                           || item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE)) {
                // create a new directory within a deleted directory? That can happen if the directory
                // etag was not fetched properly on the previous sync because the sync was aborted
                // while uploading this directory (which is now removed).  We can ignore it.
                if( delDirJob ) {
                    delDirJob->increaseAffectedCount();
                }
                continue;
            } else if (item->_instruction == CSYNC_INSTRUCTION_IGNORE) {
                continue;
            } else if (item->_instruction == CSYNC_INSTRUCTION_RENAME) {
                // all is good, the rename will be executed before the directory deletion
            } else {
                qWarning() << "WARNING:  Job within a removed directory?  This should not happen!"
                           << item->_file << item->_instruction;
            }
        }

        while (!item->destination().startsWith(directories.top().first)) {
            directories.pop();
        }

        if (item->_isDirectory) {
            PropagateDirectory *dir = new PropagateDirectory(this, item);
            dir->_firstJob.reset(createJob(item));

            if (item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE
                    && item->_direction == SyncFileItem::Up) {
                // Skip all potential uploads to the new folder.
                // Processing them now leads to problems with permissions:
                // checkForPermissions() has already run and used the permissions
                // of the file we're about to delete to decide whether uploading
                // to the new dir is ok...
                foreach(const SyncFileItemPtr &item2, items) {
                    if (item2->destination().startsWith(item->destination() + "/")) {
                        item2->_instruction = CSYNC_INSTRUCTION_NONE;
                        _anotherSyncNeeded = true;
                    }
                }
            }

            if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                // We do the removal of directories at the end, because there might be moves from
                // these directories that will happen later.
                directoriesToRemove.prepend(dir);
                removedDirectory = item->_file + "/";

                // We should not update the etag of parent directories of the removed directory
                // since it would be done before the actual remove (issue #1845)
                // NOTE: Currently this means that we don't update those etag at all in this sync,
                //       but it should not be a problem, they will be updated in the next sync.
                for (int i = 0; i < directories.size(); ++i) {
                    if (directories[i].second->_item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA)
                        directories[i].second->_item->_instruction = CSYNC_INSTRUCTION_NONE;
                }
            } else {
                PropagateDirectory* currentDirJob = directories.top().second;
                currentDirJob->append(dir);
            }
            directories.push(qMakePair(item->destination() + "/" , dir));
        } else if (PropagateItemJob* current = createJob(item)) {
            if (item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE) {
                // will delete directories, so defer execution
                directoriesToRemove.prepend(current);
                removedDirectory = item->_file + "/";
            } else {
                directories.top().second->append(current);
            }
        }
    }

    foreach(PropagatorJob* it, directoriesToRemove) {
        _rootJob->append(it);
    }

    connect(_rootJob.data(), SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)),
            this, SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)));
    connect(_rootJob.data(), SIGNAL(progress(const SyncFileItem &,quint64)), this, SIGNAL(progress(const SyncFileItem &,quint64)));
    connect(_rootJob.data(), SIGNAL(finished(SyncFileItem::Status)), this, SLOT(emitFinished(SyncFileItem::Status)));
    connect(_rootJob.data(), SIGNAL(ready()), this, SLOT(scheduleNextJob()), Qt::QueuedConnection);

    qDebug() << "Using QNAM/HTTP parallel code path";

    QTimer::singleShot(0, this, SLOT(scheduleNextJob()));
}

bool OwncloudPropagator::isInSharedDirectory(const QString& file)
{
    bool re = false;
    if( _remoteDir.contains( _account->davPath() + QLatin1String("Shared") ) ) {
        // The Shared directory is synced as its own sync connection
        re = true;
    } else {
        if( file.startsWith("Shared/") || file == "Shared" )  {
            // The whole ownCloud is synced and Shared is always a top dir
            re = true;
        }
    }
    return re;
}

int OwncloudPropagator::httpTimeout()
{
    static int timeout;
    if (!timeout) {
        timeout = qgetenv("OWNCLOUD_TIMEOUT").toUInt();
        if (timeout == 0) {
            ConfigFile cfg;
            timeout = cfg.timeout();
        }

    }
    return timeout;
}

quint64 OwncloudPropagator::chunkSize()
{
    static uint chunkSize;
    if (!chunkSize) {
        chunkSize = qgetenv("OWNCLOUD_CHUNK_SIZE").toUInt();
        if (chunkSize == 0) {
            ConfigFile cfg;
            chunkSize = cfg.chunkSize();
        }
    }
    return chunkSize;
}


bool OwncloudPropagator::localFileNameClash( const QString& relFile )
{
    bool re = false;
    const QString file( _localDir + relFile );

    if( !file.isEmpty() && Utility::fsCasePreserving() ) {
#ifdef Q_OS_MAC
        QFileInfo fileInfo(file);
        if (!fileInfo.exists()) {
            re = false;
            qDebug() << Q_FUNC_INFO << "No valid fileinfo";
        } else {
            // Need to normalize to composited form because of
            // https://bugreports.qt-project.org/browse/QTBUG-39622
            const QString cName = fileInfo.canonicalFilePath().normalized(QString::NormalizationForm_C);
            // qDebug() << Q_FUNC_INFO << "comparing " << cName << " with " << file;
            bool equal = (file == cName);
            re = (!equal && ! cName.endsWith(relFile, Qt::CaseSensitive) );
            // qDebug() << Q_FUNC_INFO << "Returning for localFileNameClash: " << re;
        }
#elif defined(Q_OS_WIN)
        const QString file( _localDir + relFile );
        qDebug() << "CaseClashCheck for " << file;
        WIN32_FIND_DATA FindFileData;
        HANDLE hFind;

        hFind = FindFirstFileW( (wchar_t*)file.utf16(), &FindFileData);
        if (hFind == INVALID_HANDLE_VALUE) {
            //qDebug() << "FindFirstFile failed " << GetLastError();
            // returns false.
        } else {
            QString realFileName = QString::fromWCharArray( FindFileData.cFileName );
            FindClose(hFind);

            if( ! file.endsWith(realFileName, Qt::CaseSensitive) ) {
                qDebug() << Q_FUNC_INFO << "Detected case clash between" << file << "and" << realFileName;
                re = true;
            }
        }
#else
        // On Linux, the file system is case sensitive, but this code is useful for testing.
        // Just check that there is no other file with the same name and different casing.
        QFileInfo fileInfo(file);
        const QString fn = fileInfo.fileName();
        QStringList list = fileInfo.dir().entryList(QStringList() << fn);
        if (list.count() > 1 || (list.count() == 1 && list[0] != fn)) {
            re = true;
        }
#endif
    }
    return re;
}

QString OwncloudPropagator::getFilePath(const QString& tmp_file_name) const
{
    return _localDir + tmp_file_name;
}

void OwncloudPropagator::scheduleNextJob()
{
    // TODO: If we see that the automatic up-scaling has a bad impact we
    // need to check how to avoid this.
    // Down-scaling on slow networks? https://github.com/owncloud/client/issues/3382
    // Making sure we do up/down at same time? https://github.com/owncloud/client/issues/1633

    if (_activeJobList.count() < maximumActiveJob()) {
        if (_rootJob->scheduleNextJob()) {
            QTimer::singleShot(0, this, SLOT(scheduleNextJob()));
        }
    } else if (_activeJobList.count() < hardMaximumActiveJob()) {
        int likelyFinishedQuicklyCount = 0;
        // NOTE: Only counts the first 3 jobs! Then for each
        // one that is likely finished quickly, we can launch another one.
        // When a job finishes another one will "move up" to be one of the first 3 and then
        // be counted too.
        for (int i = 0; i < maximumActiveJob() && i < _activeJobList.count(); i++) {
            if (_activeJobList.at(i)->isLikelyFinishedQuickly()) {
                likelyFinishedQuicklyCount++;
            }
        }
        if (_activeJobList.count() < maximumActiveJob() + likelyFinishedQuicklyCount) {
            qDebug() <<  "Can pump in another request! activeJobs =" << _activeJobList.count();
            if (_rootJob->scheduleNextJob()) {
                QTimer::singleShot(0, this, SLOT(scheduleNextJob()));
            }
        }
    }
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

// ================================================================================

PropagatorJob::JobParallelism PropagateDirectory::parallelism()
{
    // If any of the non-finished sub jobs is not parallel, we have to wait

    // FIXME!  we should probably cache this result

    if (_firstJob && _firstJob->_state != Finished) {
        if (_firstJob->parallelism() != FullParallelism)
            return WaitForFinished;
    }

    // FIXME: use the cached value of finished job
    for (int i = 0; i < _subJobs.count(); ++i) {
        if (_subJobs.at(i)->_state != Finished && _subJobs.at(i)->parallelism() != FullParallelism) {
            return WaitForFinished;
        }
    }
    return FullParallelism;
}


bool PropagateDirectory::scheduleNextJob()
{
    if (_state == Finished) {
        return false;
    }

    if (_state == NotYetStarted) {
        _state = Running;

        if (!_firstJob && _subJobs.isEmpty()) {
            finalize();
            return true;
        }
    }

    if (_firstJob && _firstJob->_state == NotYetStarted) {
        return possiblyRunNextJob(_firstJob.data());
    }

    if (_firstJob && _firstJob->_state == Running) {
        return false;
    }

    // cache the value of first unfinished subjob
    bool stopAtDirectory = false;
    int i = _firstUnfinishedSubJob;
    int subJobsCount = _subJobs.count();
    while (i < subJobsCount && _subJobs.at(i)->_state == Finished) {
      _firstUnfinishedSubJob = ++i;
    }

    for (int i = _firstUnfinishedSubJob; i < subJobsCount; ++i) {
        if (_subJobs.at(i)->_state == Finished) {
            continue;
        }

        if (stopAtDirectory && qobject_cast<PropagateDirectory*>(_subJobs.at(i))) {
            return false;
        }

        if (possiblyRunNextJob(_subJobs.at(i))) {
            return true;
        }

        Q_ASSERT(_subJobs.at(i)->_state == Running);

        auto paral = _subJobs.at(i)->parallelism();
        if (paral == WaitForFinished) {
            return false;
        }
        if (paral == WaitForFinishedInParentDirectory) {
            stopAtDirectory = true;
        }
    }
    return false;
}

void PropagateDirectory::slotSubJobFinished(SyncFileItem::Status status)
{
    if (status == SyncFileItem::FatalError ||
            (sender() == _firstJob.data() && status != SyncFileItem::Success && status != SyncFileItem::Restoration)) {
        abort();
        _state = Finished;
        emit finished(status);
        return;
    } else if (status == SyncFileItem::NormalError || status == SyncFileItem::SoftError) {
        _hasError = status;
    }
    _runningNow--;
    _jobsFinished++;

    int totalJobs = _subJobs.count();
    if (_firstJob) {
        totalJobs++;
    }

    // We finished processing all the jobs
    // check if we finished
    if (_jobsFinished >= totalJobs) {
        Q_ASSERT(!_runningNow); // how can we be finished if there are still jobs running now
        finalize();
    } else {
        emit ready();
    }
}

void PropagateDirectory::finalize()
{
    bool ok = true;
    if (!_item->isEmpty() && _hasError == SyncFileItem::NoStatus) {
        if( !_item->_renameTarget.isEmpty() ) {
            if(_item->_instruction == CSYNC_INSTRUCTION_RENAME
                    && _item->_originalFile != _item->_renameTarget) {
                // Remove the stale entries from the database.
                _propagator->_journal->deleteFileRecord(_item->_originalFile, true);
            }

            _item->_file = _item->_renameTarget;
        }

        // For new directories we always want to update the etag once
        // the directory has been propagated. Otherwise the directory
        // could appear locally without being added to the database.
        if (_item->_instruction == CSYNC_INSTRUCTION_RENAME
            || _item->_instruction == CSYNC_INSTRUCTION_NEW
            || _item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA) {
            if (PropagateRemoteMkdir* mkdir = qobject_cast<PropagateRemoteMkdir*>(_firstJob.data())) {
                // special case from MKDIR, get the fileId from the job there
                if (_item->_fileId.isEmpty() && !mkdir->_item->_fileId.isEmpty()) {
                    _item->_fileId = mkdir->_item->_fileId;
                }
            }
            SyncJournalFileRecord record(*_item,  _propagator->_localDir + _item->_file);
            ok = _propagator->_journal->setFileRecordMetadata(record);
            if (!ok) {
                _hasError = _item->_status = SyncFileItem::FatalError;
                _item->_errorString = tr("Error writing metadata to the database");
                qWarning() << "Error writing to the database for file" << _item->_file;
            }
        }
    }
    _state = Finished;
    emit finished(_hasError == SyncFileItem::NoStatus ?  SyncFileItem::Success : _hasError);
}

qint64 PropagateDirectory::committedDiskSpace() const
{
    qint64 needed = 0;
    foreach (PropagatorJob* job, _subJobs) {
        needed += job->committedDiskSpace();
    }
    return needed;
}

CleanupPollsJob::~CleanupPollsJob()
{}

void CleanupPollsJob::start()
{
    if (_pollInfos.empty()) {
        emit finished();
        deleteLater();
        return;
    }

    auto info = _pollInfos.first();
    _pollInfos.pop_front();
    SyncJournalFileRecord record = _journal->getFileRecord(info._file);
    SyncFileItemPtr item(new SyncFileItem(record.toSyncFileItem()));
    if (record.isValid()) {
        PollJob *job = new PollJob(_account, info._url, item, _journal, _localPath, this);
        connect(job, SIGNAL(finishedSignal()), SLOT(slotPollFinished()));
        job->start();
    }
}

void CleanupPollsJob::slotPollFinished()
{
    PollJob *job = qobject_cast<PollJob *>(sender());
    Q_ASSERT(job);
    if (job->_item->_status == SyncFileItem::FatalError) {
        emit aborted(job->_item->_errorString);
        deleteLater();
        return;
    } else if (job->_item->_status != SyncFileItem::Success) {
        qDebug() << "There was an error with file " << job->_item->_file << job->_item->_errorString;
    } else {
        if (!_journal->setFileRecord(SyncJournalFileRecord(*job->_item, _localPath + job->_item->_file))) {
            qWarning() << "database error";
            job->_item->_status = SyncFileItem::FatalError;
            job->_item->_errorString = tr("Error writing metadata to the database");
            emit aborted(job->_item->_errorString);
            deleteLater();
            return;
        }
    }
    // Continue with the next entry, or finish
    start();
}

}

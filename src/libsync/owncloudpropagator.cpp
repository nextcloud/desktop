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
#ifdef USE_NEON
#include "propagator_legacy.h"
#endif
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

OwncloudPropagator::~OwncloudPropagator()
{}

/* The maximum number of active job in parallel  */
int OwncloudPropagator::maximumActiveJob()
{
    static int max = qgetenv("OWNCLOUD_MAX_PARALLEL").toUInt();
    if (!max) {
        max = 3; //default
    }
    return max;
}

/** Updates or creates a blacklist entry for the given item.
 *
 * Returns whether the file is in the blacklist now.
 */
static bool blacklist(SyncJournalDb* journal, const SyncFileItem& item)
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
        if (blacklist(_propagator->_journal, *_item) && _item->_hasBlacklistEntry) {
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

    emit completed(*_item);
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
            if (downloadItem->_instruction == CSYNC_INSTRUCTION_NEW) {
                // don't try to recover pushing new files
                return false;
            } else if (downloadItem->_instruction == CSYNC_INSTRUCTION_SYNC) {
                // we modified the file locally, jsut create a conflict then
                downloadItem->_instruction = CSYNC_INSTRUCTION_CONFLICT;

                // HACK to avoid continuation: See task #1448:  We do not know the _modtime from the
                //  server, at this point, so just set the current one. (rather than the one locally)
                downloadItem->_modtime = Utility::qDateTimeToTime_t(QDateTime::currentDateTime());
            } else {
                // the file was removed or renamed, just recover the old one
                downloadItem->_instruction = CSYNC_INSTRUCTION_SYNC;
            }
            downloadItem->_direction = SyncFileItem::Down;
#ifdef USE_NEON
            newJob = new PropagateDownloadFileLegacy(_propagator, downloadItem);
#else
            newJob = new PropagateDownloadFileQNAM(_propagator, downloadItem);
#endif
        } else {
            // Directories are harder to recover.
            // But just re-create the directory, next sync will be able to recover the files
            SyncFileItemPtr mkdirItem(new SyncFileItem(*_item));
            mkdirItem->_instruction = CSYNC_INSTRUCTION_SYNC;
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
            connect(_restoreJob.data(), SIGNAL(completed(const SyncFileItemPtr &)),
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
        done( item._status, tr("A file or directory was removed from a read only share, but restoring failed: %1").arg(item._errorString) );
    }
}

// ================================================================================

PropagateItemJob* OwncloudPropagator::createJob(const SyncFileItemPtr &item) {
    switch(item->_instruction) {
        case CSYNC_INSTRUCTION_REMOVE:
            if (item->_direction == SyncFileItem::Down) return new PropagateLocalRemove(this, item);
            else return new PropagateRemoteDelete(this, item);
        case CSYNC_INSTRUCTION_NEW:
            if (item->_isDirectory) {
                if (item->_direction == SyncFileItem::Down) return new PropagateLocalMkdir(this, item);
                else return new PropagateRemoteMkdir(this, item);
            }   //fall trough
        case CSYNC_INSTRUCTION_SYNC:
        case CSYNC_INSTRUCTION_CONFLICT:
            if (item->_isDirectory) {
                // Should we set the mtime?
                return 0;
            }
#ifdef USE_NEON
            if (useLegacyJobs()) {
                if (item->_direction != SyncFileItem::Up) {
                    return new PropagateDownloadFileLegacy(this, item);
                } else {
                    return new PropagateUploadFileLegacy(this, item);
                }
            } else
#endif
            {
                if (item->_direction != SyncFileItem::Up) {
                    return new PropagateDownloadFileQNAM(this, item);
                } else {
                    return new PropagateUploadFileQNAM(this, item);
                }
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

    /* Check and log the transmission checksum type */
    ConfigFile cfg;
    const QString checksumType = cfg.transmissionChecksum().toUpper();

    /* if the checksum type is empty, it is not send. No error */
    if( !checksumType.isEmpty() ) {
        if( checksumType == checkSumAdlerUpperC ||
                checksumType == checkSumMD5C    ||
                checksumType == checkSumSHA1C ) {
            qDebug() << "Client sends and expects transmission checksum type" << checksumType;
        } else {
            qWarning() << "Unknown transmission checksum type from config" << checksumType;
        }
    }

    /* This builds all the job needed for the propagation.
     * Each directories is a PropagateDirectory job, which contains the files in it.
     * In order to do that we loop over the items. (which are sorted by destination)
     * When we enter adirectory, we can create the directory job and push it on the stack. */

    _rootJob.reset(new PropagateDirectory(this));
    QStack<QPair<QString /* directory name */, PropagateDirectory* /* job */> > directories;
    directories.push(qMakePair(QString(), _rootJob.data()));
    QVector<PropagatorJob*> directoriesToRemove;
    QString removedDirectory;
    foreach(const SyncFileItemPtr &item, items) {

        if (!removedDirectory.isEmpty() && item->_file.startsWith(removedDirectory)) {
            // this is an item in a directory which is going to be removed.
            PropagateDirectory *delDirJob = dynamic_cast<PropagateDirectory*>(directoriesToRemove.last());

            if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                //already taken care of.  (by the removal of the parent directory)

                // increase the number of subjobs that would be there.
                if( delDirJob ) {
                    delDirJob->increaseAffectedCount();
                }
                continue;
            } else if (item->_instruction == CSYNC_INSTRUCTION_NEW && item->_isDirectory) {
                // create a new directory within a deleted directory? That can happen if the directory
                // etag were not fetched properly on the previous sync because the sync was aborted
                // while uploading this directory (which is now removed).  We can ignore it.
                if( delDirJob ) {
                    delDirJob->increaseAffectedCount();
                }
                continue;
            } else if (item->_instruction == CSYNC_INSTRUCTION_IGNORE) {
                continue;
            }

            qWarning() << "WARNING:  Job within a removed directory?  This should not happen!"
                       << item->_file << item->_instruction;
        }

        while (!item->destination().startsWith(directories.top().first)) {
            directories.pop();
        }

        if (item->_isDirectory) {
            PropagateDirectory *dir = new PropagateDirectory(this, item);
            dir->_firstJob.reset(createJob(item));
            if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                //We do the removal of directories at the end, because there might be moves from
                // this directories that will happen later.
                directoriesToRemove.append(dir);
                removedDirectory = item->_file + "/";

                // We should not update the etag of parent directories of the removed directory
                // since it would be done before the actual remove (issue #1845)
                // NOTE: Currently this means that we don't update those etag at all in this sync,
                //       but it should not be a problem, they will be updated in the next sync.
                for (int i = 0; i < directories.size(); ++i) {
                    directories[i].second->_item->_should_update_metadata = false;
                }
            } else {
                PropagateDirectory* currentDirJob = directories.top().second;
                currentDirJob->append(dir);
            }
            directories.push(qMakePair(item->destination() + "/" , dir));
        } else if (PropagateItemJob* current = createJob(item)) {
            directories.top().second->append(current);
        }
    }

    foreach(PropagatorJob* it, directoriesToRemove) {
        _rootJob->append(it);
    }

    connect(_rootJob.data(), SIGNAL(completed(const SyncFileItem &)), this, SIGNAL(completed(const SyncFileItem &)));
    connect(_rootJob.data(), SIGNAL(progress(const SyncFileItem &,quint64)), this, SIGNAL(progress(const SyncFileItem &,quint64)));
    connect(_rootJob.data(), SIGNAL(finished(SyncFileItem::Status)), this, SLOT(emitFinished()));
    connect(_rootJob.data(), SIGNAL(ready()), this, SLOT(scheduleNextJob()), Qt::QueuedConnection);

    qDebug() << (useLegacyJobs() ? "Using legacy libneon/HTTP sequential code path" : "Using QNAM/HTTP parallel code path");

    QTimer::singleShot(0, this, SLOT(scheduleNextJob()));
}

bool OwncloudPropagator::isInSharedDirectory(const QString& file)
{
    bool re = false;
    if( _remoteDir.contains("remote.php/webdav/Shared") ) {
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

/**
 * Return true if we should use the legacy jobs.
 * Some feature are not supported by QNAM and therefore we still use the legacy jobs
 * for this case.
 */
bool OwncloudPropagator::useLegacyJobs()
{
#ifdef USE_NEON
    // Allow an environement variable for debugging
    QByteArray env = qgetenv("OWNCLOUD_USE_LEGACY_JOBS");
    if (env=="true" || env =="1") {
        qDebug() << "Force Legacy Propagator ACTIVATED";
        return true;
    }

    if (_downloadLimit.fetchAndAddAcquire(0) != 0 || _uploadLimit.fetchAndAddAcquire(0) != 0) {
        // QNAM bandwith limiting only work with version of Qt greater or equal to 5.3.3
        // (It needs Qt commits 097b641 and b99fa32)
#if QT_VERSION >= QT_VERSION_CHECK(5,3,3)
        return false;
#elif QT_VERSION >= QT_VERSION_CHECK(5,0,0)
        env = qgetenv("OWNCLOUD_NEW_BANDWIDTH_LIMITING");
        if (env=="true" || env =="1") {
            qDebug() << "New Bandwidth Limiting Code ACTIVATED";
            return false;
        }

        // Do a runtime check.
        // (Poor man's version comparison)
        const char *v = qVersion(); // "x.y.z";
        if (QLatin1String(v) >= QLatin1String("5.3.3")) {
            return false;
        } else {
            qDebug() << "Use legacy jobs because qt version is only" << v << "while 5.3.3 is needed";
            return true;
        }
#else
        qDebug() << "Use legacy jobs because of Qt4";
        return true;
#endif
    }
#endif // USE_NEON
    return false;
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
        // On Linux, the file system is case sensitive, but this code is usefull for testing.
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
    if (this->_activeJobs < maximumActiveJob()) {
        if (_rootJob->scheduleNextJob()) {
            QTimer::singleShot(100, this, SLOT(scheduleNextJob()));
        }
    }
}

void OwncloudPropagator::addTouchedFile(const QString& fn)
{
    QString file = QDir::cleanPath(fn);

    QElapsedTimer timer;
    timer.start();

    QMutexLocker lock(&_touchedFilesMutex);
    _touchedFiles.insert(file, timer);
}

qint64 OwncloudPropagator::timeSinceFileTouched(const QString& fn) const
{
    QMutexLocker lock(&_touchedFilesMutex);
    if (! _touchedFiles.contains(fn)) {
        return -1;
    }

    return _touchedFiles[fn].elapsed();
}

AccountPtr OwncloudPropagator::account() const
{
    return _account;
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

    bool stopAtDirectory = false;
    // FIXME: use the cached value of finished job
    for (int i = 0; i < _subJobs.count(); ++i) {
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

    int total = _subJobs.count();
    if (!_firstJob) {
        total--;
    }

    _current++;

    // We finished to processing all the jobs
    // check if we finished
    if (_current >= total) {
        Q_ASSERT(!_runningNow); // how can we finished if there are still jobs running now
        finalize();
    } else {
        emit ready();
    }
}

void PropagateDirectory::finalize()
{
    if (!_item->isEmpty() && _hasError == SyncFileItem::NoStatus) {
        if( !_item->_renameTarget.isEmpty() ) {
            _item->_file = _item->_renameTarget;
        }

        if (_item->_should_update_metadata && _item->_instruction != CSYNC_INSTRUCTION_REMOVE) {
            if (PropagateRemoteMkdir* mkdir = qobject_cast<PropagateRemoteMkdir*>(_firstJob.data())) {
                // special case from MKDIR, get the fileId from the job there
                if (_item->_fileId.isEmpty() && !mkdir->_item->_fileId.isEmpty()) {
                    _item->_fileId = mkdir->_item->_fileId;
                }
            }
            SyncJournalFileRecord record(*_item,  _propagator->_localDir + _item->_file);
            _propagator->_journal->setFileRecord(record);
        }
    }
    _state = Finished;
    emit finished(_hasError == SyncFileItem::NoStatus ? SyncFileItem::Success : _hasError);
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
    SyncFileItemPtr item(new SyncFileItem);
    item->_file = info._file;
    item->_modtime = info._modtime;
    PollJob *job = new PollJob(_account, info._url, item, _journal, _localPath, this);
    connect(job, SIGNAL(finishedSignal()), SLOT(slotPollFinished()));
    job->start();
}

void CleanupPollsJob::slotPollFinished()
{
    PollJob *job = qobject_cast<PollJob *>(sender());
    Q_ASSERT(job);
    if (job->_item->_status == SyncFileItem::FatalError) {
        emit aborted(job->_item->_errorString);
        return;
    } else if (job->_item->_status != SyncFileItem::Success) {
        qDebug() << "There was an error with file " << job->_item->_file << job->_item->_errorString;
    } else {
        _journal->setFileRecord(SyncJournalFileRecord(*job->_item, _localPath + job->_item->_file));
    }
    // Continue with the next entry, or finish
    start();
}

}

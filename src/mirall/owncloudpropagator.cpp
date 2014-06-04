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
#include "propagator_qnam.h"
#include "propagatorjobs.h"
#include "propagator_legacy.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/utility.h"

#ifdef Q_OS_WIN
#include <windef.h>
#include <winbase.h>
#endif

#include <QStack>
#include <QFileInfo>

namespace Mirall {

/* The maximum number of active job in parallel  */
static int maximumActiveJob() {
    static int max = qgetenv("OWNCLOUD_MAX_PARALLEL").toUInt();
    if (!max) {
        max = 3; //default
    }
    return max;
}

void PropagateItemJob::done(SyncFileItem::Status status, const QString &errorString)
{
    _item._errorString = errorString;
    _item._status = status;

    // Blacklisting
    int retries = 0;

    if( _item._httpErrorCode == 403 ||_item._httpErrorCode == 413 || _item._httpErrorCode == 415 ) {
        qDebug() << "Fatal Error condition" << _item._httpErrorCode << ", forbid retry!";
        retries = -1;
    } else {
        static QAtomicInt defaultRetriesCount(qgetenv("OWNCLOUD_BLACKLIST_COUNT").toInt());
        if (defaultRetriesCount.fetchAndAddAcquire(0) <= 0) {
            defaultRetriesCount.fetchAndStoreRelease(3);
        }
        retries = defaultRetriesCount.fetchAndAddAcquire(0);
    }
    SyncJournalBlacklistRecord record(_item, retries);;

    switch( status ) {
    case SyncFileItem::SoftError:
    case SyncFileItem::FatalError:
        // do not blacklist in case of soft error or fatal error.
        break;
    case SyncFileItem::NormalError:
#ifdef OWNCLOUD_5XX_NO_BLACKLIST
        if (_item._httpErrorCode / 100 == 5) {
            // In this configuration, never blacklist error 5xx
            qDebug() << "Do not blacklist error " << _item._httpErrorCode;
            break;
        }
#endif
        _propagator->_journal->updateBlacklistEntry( record );
        break;
    case SyncFileItem::Success:
        if( _item._blacklistedInDb ) {
            // wipe blacklist entry.
            _propagator->_journal->wipeBlacklistEntry(_item._file);
        }
        break;
    case SyncFileItem::Conflict:
    case SyncFileItem::FileIgnored:
    case SyncFileItem::NoStatus:
        // nothing
        break;
    }

    emit completed(_item);
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

    if( httpStatusCode == 403 && _propagator->isInSharedDirectory(_item._file )) {
        if( !_item._isDirectory ) {
            SyncFileItem downloadItem(_item);
            if (downloadItem._instruction == CSYNC_INSTRUCTION_NEW) {
                // don't try to recover pushing new files
                return false;
            } else if (downloadItem._instruction == CSYNC_INSTRUCTION_SYNC) {
                // we modified the file locally, jsut create a conflict then
                downloadItem._instruction = CSYNC_INSTRUCTION_CONFLICT;

                // HACK to avoid continuation: See task #1448:  We do not know the _modtime from the
                //  server, at this point, so just set the current one. (rather than the one locally)
                downloadItem._modtime = Utility::qDateTimeToTime_t(QDateTime::currentDateTime());
            } else {
                // the file was removed or renamed, just recover the old one
                downloadItem._instruction = CSYNC_INSTRUCTION_SYNC;
            }
            downloadItem._direction = SyncFileItem::Down;
            newJob = new PropagateDownloadFileLegacy(_propagator, downloadItem);
        } else {
            // Directories are harder to recover.
            // But just re-create the directory, next sync will be able to recover the files
            SyncFileItem mkdirItem(_item);
            mkdirItem._instruction = CSYNC_INSTRUCTION_SYNC;
            mkdirItem._direction = SyncFileItem::Down;
            newJob = new PropagateLocalMkdir(_propagator, mkdirItem);
            // Also remove the inodes and fileid from the db so no further renames are tried for
            // this item.
            _propagator->_journal->avoidRenamesOnNextSync(_item._file);
        }
        if( newJob )  {
            newJob->setRestoreJobMsg(msg);
            _restoreJob.reset(newJob);
            connect(_restoreJob.data(), SIGNAL(completed(SyncFileItem)),
                    this, SLOT(slotRestoreJobCompleted(SyncFileItem)));
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

    if( item._status == SyncFileItem::Success ||  item._status == SyncFileItem::Conflict) {
        done( SyncFileItem::SoftError, msg);
    } else {
        done( item._status, tr("A file or directory was removed from a read only share, but restoring failed: %1").arg(item._errorString) );
    }
}

// ================================================================================

PropagateItemJob* OwncloudPropagator::createJob(const SyncFileItem& item) {
    switch(item._instruction) {
        case CSYNC_INSTRUCTION_REMOVE:
            if (item._direction == SyncFileItem::Down) return new PropagateLocalRemove(this, item);
            else return new PropagateRemoteRemove(this, item);
        case CSYNC_INSTRUCTION_NEW:
            if (item._isDirectory) {
                if (item._direction == SyncFileItem::Down) return new PropagateLocalMkdir(this, item);
                else return new PropagateRemoteMkdir(this, item);
            }   //fall trough
        case CSYNC_INSTRUCTION_SYNC:
        case CSYNC_INSTRUCTION_CONFLICT:
            if (item._isDirectory) {
                // Should we set the mtime?
                return 0;
            }
            if (useLegacyJobs()) {
                if (item._direction != SyncFileItem::Up) {
                    return new PropagateDownloadFileLegacy(this, item);
                } else {
                    return new PropagateUploadFileLegacy(this, item);
                }
            } else {
                if (item._direction != SyncFileItem::Up) {
                    return new PropagateDownloadFileQNAM(this, item);
                } else {
                    return new PropagateUploadFileQNAM(this, item);
                }
            }
        case CSYNC_INSTRUCTION_RENAME:
            if (item._direction == SyncFileItem::Up) {
                return new PropagateRemoteRename(this, item);
            } else {
                return new PropagateLocalRename(this, item);
            }
        case CSYNC_INSTRUCTION_IGNORE:
            return new PropagateIgnoreJob(this, item);
        default:
            return 0;
    }
    return 0;
}

void OwncloudPropagator::start(const SyncFileItemVector& _syncedItems)
{
    /* This builds all the job needed for the propagation.
     * Each directories is a PropagateDirectory job, which contains the files in it.
     * In order to do that we sort the items by destination. and loop over it. When we enter a
     * directory, we can create the directory job and push it on the stack. */
    SyncFileItemVector items = _syncedItems;
    std::sort(items.begin(), items.end());
    _rootJob.reset(new PropagateDirectory(this));
    QStack<QPair<QString /* directory name */, PropagateDirectory* /* job */> > directories;
    directories.push(qMakePair(QString(), _rootJob.data()));
    QVector<PropagatorJob*> directoriesToRemove;
    QString removedDirectory;
    foreach(const SyncFileItem &item, items) {

        if (!removedDirectory.isEmpty() && item._file.startsWith(removedDirectory)) {
            // this is an item in a directory which is going to be removed.
            if (item._instruction == CSYNC_INSTRUCTION_REMOVE) {
                //already taken care of.  (by the removal of the parent directory)
                continue;
            } else if (item._instruction == CSYNC_INSTRUCTION_NEW && item._isDirectory) {
                // create a new directory within a deleted directory? That can happen if the directory
                // etag were not fetched properly on the previous sync because the sync was aborted
                // while uploading this directory (which is now removed).  We can ignore it.
                continue;
            } else if (item._instruction == CSYNC_INSTRUCTION_IGNORE) {
                continue;
            }

            qWarning() << "WARNING:  Job within a removed directory?  This should not happen!"
                       << item._file << item._instruction;
        }

        while (!item.destination().startsWith(directories.top().first)) {
            directories.pop();
        }

        if (item._isDirectory) {
            PropagateDirectory *dir = new PropagateDirectory(this, item);
            dir->_firstJob.reset(createJob(item));
            if (item._instruction == CSYNC_INSTRUCTION_REMOVE) {
                //We do the removal of directories at the end, because there might be moves from
                // this directories that will happen later.
                directoriesToRemove.append(dir);
                removedDirectory = item._file + "/";

                // We should not update the etag of parent directories of the removed directory
                // since it would be done before the actual remove (issue #1845)
                // NOTE: Currently this means that we don't update those etag at all in this sync,
                //       but it should not be a problem, they will be updated in the next sync.
                for (int i = 0; i < directories.size(); ++i) {
                    directories[i].second->_item._should_update_etag = false;
                }
            } else {
                PropagateDirectory* currentDirJob = directories.top().second;
                currentDirJob->append(dir);
            }
            directories.push(qMakePair(item.destination() + "/" , dir));
        } else if (PropagateItemJob* current = createJob(item)) {
            directories.top().second->append(current);
        }
    }

    foreach(PropagatorJob* it, directoriesToRemove) {
        _rootJob->append(it);
    }

    connect(_rootJob.data(), SIGNAL(completed(SyncFileItem)), this, SIGNAL(completed(SyncFileItem)));
    connect(_rootJob.data(), SIGNAL(progress(SyncFileItem,quint64)), this, SIGNAL(progress(SyncFileItem,quint64)));
    connect(_rootJob.data(), SIGNAL(finished(SyncFileItem::Status)), this, SLOT(emitFinished()));

    qDebug() << (useLegacyJobs() ? "Using legacy libneon/HTTP sequential code path" : "Using QNAM/HTTP parallel code path");

    QMetaObject::invokeMethod(_rootJob.data(), "start", Qt::QueuedConnection);
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
    if (_downloadLimit.fetchAndAddAcquire(0) != 0 || _uploadLimit.fetchAndAddAcquire(0) != 0) {
        // QNAM does not support bandwith limiting
        return true;
    }

    // Allow an environement variable for debugging
    QByteArray env = qgetenv("OWNCLOUD_USE_LEGACY_JOBS");
    return env=="true" || env =="1";
}

int OwncloudPropagator::httpTimeout()
{
    static int timeout;
    if (!timeout) {
        timeout = qgetenv("OWNCLOUD_TIMEOUT").toUInt();
        if (timeout == 0) {
            MirallConfigFile cfg;
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
        } else {
            re = ( ! fileInfo.canonicalFilePath().endsWith(relFile, Qt::CaseSensitive) );
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
            qDebug() << Q_FUNC_INFO << "Real file name is " << realFileName;
            FindClose(hFind);

            if( ! file.endsWith(realFileName, Qt::CaseSensitive) ) {
                re = true;
            }
        }
#endif
    }
    return re;
}

// ================================================================================

void PropagateDirectory::start()
{
    _current = -1;
    _hasError = SyncFileItem::NoStatus;
    if (!_firstJob) {
        slotSubJobReady();
    } else {
        startJob(_firstJob.data());
    }
}

void PropagateDirectory::slotSubJobFinished(SyncFileItem::Status status)
{
    if (status == SyncFileItem::FatalError || (_current == -1 && status != SyncFileItem::Success)) {
        abort();
        emit finished(status);
        return;
    } else if (status == SyncFileItem::NormalError || status == SyncFileItem::SoftError) {
        _hasError = status;
    }
    _runningNow--;
    slotSubJobReady();
}

void PropagateDirectory::slotSubJobReady()
{
    if (_runningNow && _current == -1)
        return; // Ignore the case when the _fistJob is ready and not yet finished
    if (_runningNow && _current >= 0 && _current < _subJobs.count()) {
        // there is a job running and the current one is not ready yet, we can't start new job
        if (!_subJobs[_current]->_readySent || _propagator->_activeJobs >= maximumActiveJob())
            return;
    }

    _current++;
    if (_current < _subJobs.size() && !_propagator->_abortRequested.fetchAndAddRelaxed(0)) {
        PropagatorJob *next = _subJobs.at(_current);
        startJob(next);
        return;
    }
    // We finished to processing all the jobs
    emitReady();
    if (!_runningNow) {
        if (!_item.isEmpty() && _hasError == SyncFileItem::NoStatus) {
            if( !_item._renameTarget.isEmpty() ) {
                _item._file = _item._renameTarget;
            }

            if (_item._should_update_etag && _item._instruction != CSYNC_INSTRUCTION_REMOVE) {
                SyncJournalFileRecord record(_item,  _propagator->_localDir + _item._file);
                _propagator->_journal->setFileRecord(record);
            }
        }
        emit finished(_hasError == SyncFileItem::NoStatus ? SyncFileItem::Success : _hasError);
    }
}

}

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

#include <QStack>

namespace Mirall {

/* The maximum number of active job in parallel  */
static const int maximumActiveJob = 6;

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
        retries = 3; // FIXME: good number of allowed retries?
    }
    SyncJournalBlacklistRecord record(_item, retries);;

    switch( status ) {
    case SyncFileItem::SoftError:
        // do not blacklist in case of soft error.
        emit progress( Progress::SoftError, _item, 0, 0 );
        break;
    case SyncFileItem::FatalError:
    case SyncFileItem::NormalError:
        _propagator->_journal->updateBlacklistEntry( record );
        if( status == SyncFileItem::NormalError ) {
            emit progress( Progress::NormalError, _item, 0, 0 );
        }
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
        if( _item._type != SyncFileItem::Directory ) {
            SyncFileItem downloadItem(_item);
            if (downloadItem._instruction == CSYNC_INSTRUCTION_NEW) {
                // don't try to recover pushing new files
                return false;
            } else if (downloadItem._instruction == CSYNC_INSTRUCTION_SYNC) {
                // we modified the file locally, jsut create a conflict then
                downloadItem._instruction = CSYNC_INSTRUCTION_CONFLICT;
            } else {
                // the file was removed or renamed, just recover the old one
                downloadItem._instruction = CSYNC_INSTRUCTION_SYNC;
            }
            downloadItem._dir = SyncFileItem::Down;
            newJob = new PropagateDownloadFileLegacy(_propagator, downloadItem);
        } else {
            // Directories are harder to recover.
            // But just re-create the directory, next sync will be able to recover the files
            SyncFileItem mkdirItem(_item);
            mkdirItem._instruction = CSYNC_INSTRUCTION_SYNC;
            mkdirItem._dir = SyncFileItem::Down;
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
            _restoreJob->start();
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

PropagateItemJob* OwncloudPropagator::createJob(const SyncFileItem& item) {
    switch(item._instruction) {
        case CSYNC_INSTRUCTION_REMOVE:
            if (item._dir == SyncFileItem::Down) return new PropagateLocalRemove(this, item);
            else return new PropagateRemoteRemove(this, item);
        case CSYNC_INSTRUCTION_NEW:
            if (item._isDirectory) {
                if (item._dir == SyncFileItem::Down) return new PropagateLocalMkdir(this, item);
                else return new PropagateRemoteMkdir(this, item);
            }   //fall trough
        case CSYNC_INSTRUCTION_SYNC:
        case CSYNC_INSTRUCTION_CONFLICT:
            if (item._isDirectory) {
                // Should we set the mtime?
                return 0;
            }
            if (useLegacyJobs()) {
                if (item._dir != SyncFileItem::Up) {
                    return new PropagateDownloadFileLegacy(this, item);
                } else {
                    return new PropagateUploadFileLegacy(this, item);
                }
            } else {
                if (item._dir != SyncFileItem::Up) {
                    return new PropagateDownloadFileQNAM(this, item);
                } else {
                    return new PropagateUploadFileQNAM(this, item);
                }
            }
        case CSYNC_INSTRUCTION_RENAME:
            if (item._dir == SyncFileItem::Up) {
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
        if (item._instruction == CSYNC_INSTRUCTION_REMOVE
            && !removedDirectory.isEmpty() && item._file.startsWith(removedDirectory)) {
            //already taken care of.  (by the removal of the parent directory)
            continue;
        }

        while (!item.destination().startsWith(directories.top().first)) {
            directories.pop();
        }

        if (item._isDirectory) {
            PropagateDirectory *dir = new PropagateDirectory(this, item);
            dir->_firstJob.reset(createJob(item));
            if (item._instruction == CSYNC_INSTRUCTION_REMOVE) {
                //We do the removal of directories at the end
                directoriesToRemove.append(dir);
                removedDirectory = item._file + "/";
            } else {
                directories.top().second->append(dir);
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
    connect(_rootJob.data(), SIGNAL(progress(Progress::Kind,SyncFileItem,quint64,quint64)), this,
            SIGNAL(progress(Progress::Kind,SyncFileItem,quint64,quint64)));
    connect(_rootJob.data(), SIGNAL(finished(SyncFileItem::Status)), this, SIGNAL(finished()));

    QMetaObject::invokeMethod(_rootJob.data(), "start");
}

void OwncloudPropagator::overallTransmissionSizeChanged(qint64 change)
{
    emit progressChanged(change);
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
    qDebug() << Q_FUNC_INFO << _runningNow << _propagator->_activeJobs;

    if (_runningNow && _current == -1)
        return; // Ignore the case when the _fistJob is ready and not yet finished
    if (_runningNow && _current >= 0 && _current < _subJobs.count()) {
        // there is a job running and the current one is not ready yet, we can't start new job
        qDebug() <<  _subJobs[_current]->_readySent << maximumActiveJob << _subJobs[_current];
        if (!_subJobs[_current]->_readySent || _propagator->_activeJobs >= maximumActiveJob)
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

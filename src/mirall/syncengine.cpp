/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include "mirall/syncengine.h"
#include "mirall/account.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"
#include "mirall/logger.h"
#include "owncloudpropagator.h"
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include "creds/abstractcredentials.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <assert.h>

#include <QDebug>
#include <QSslSocket>
#include <QDir>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>
#include <QTime>
#include <QUrl>
#include <QSslCertificate>

namespace Mirall {

void csyncLogCatcher(int /*verbosity*/,
                     const char */*function*/,
                     const char *buffer,
                     void */*userdata*/)
{
  Logger::instance()->csyncLog( QString::fromUtf8(buffer) );
}

/* static variables to hold the credentials */
QMutex SyncEngine::_syncMutex;

SyncEngine::SyncEngine(CSYNC *ctx, const QString& localPath, const QString& remoteURL, const QString& remotePath, Mirall::SyncJournalDb* journal)
{
    _localPath = localPath;
    _remotePath = remotePath;
    _remoteUrl = remoteURL;
    _csync_ctx = ctx;
    _journal = journal;
    qRegisterMetaType<SyncFileItem>("SyncFileItem");
    qRegisterMetaType<SyncFileItem::Status>("SyncFileItem::Status");
    qRegisterMetaType<Progress::Info>("Progress::Info");

    _thread.setObjectName("CSync_Neon_Thread");
    _thread.start();
}

SyncEngine::~SyncEngine()
{
    _thread.quit();
    _thread.wait();
}

//Convert an error code from csync to a user readable string.
// Keep that function thread safe as it can be called from the sync thread or the main thread
QString SyncEngine::csyncErrorToString(CSYNC_STATUS err)
{
    QString errStr;

    switch( err ) {
    case CSYNC_STATUS_OK:
        errStr = tr("Success.");
        break;
    case CSYNC_STATUS_NO_LOCK:
        errStr = tr("CSync failed to create a lock file.");
        break;
    case CSYNC_STATUS_STATEDB_LOAD_ERROR:
        errStr = tr("CSync failed to load or create the journal file. "
                    "Make sure you have read and write permissions in the local sync directory.");
        break;
    case CSYNC_STATUS_STATEDB_WRITE_ERROR:
        errStr = tr("CSync failed to write the journal file.");
        break;
    case CSYNC_STATUS_NO_MODULE:
        errStr = tr("<p>The %1 plugin for csync could not be loaded.<br/>Please verify the installation!</p>").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_STATUS_TIMESKEW:
        errStr = tr("The system time on this client is different than the system time on the server. "
                    "Please use a time synchronization service (NTP) on the server and client machines "
                    "so that the times remain the same.");
        break;
    case CSYNC_STATUS_FILESYSTEM_UNKNOWN:
        errStr = tr("CSync could not detect the filesystem type.");
        break;
    case CSYNC_STATUS_TREE_ERROR:
        errStr = tr("CSync got an error while processing internal trees.");
        break;
    case CSYNC_STATUS_MEMORY_ERROR:
        errStr = tr("CSync failed to reserve memory.");
        break;
    case CSYNC_STATUS_PARAM_ERROR:
        errStr = tr("CSync fatal parameter error.");
        break;
    case CSYNC_STATUS_UPDATE_ERROR:
        errStr = tr("CSync processing step update failed.");
        break;
    case CSYNC_STATUS_RECONCILE_ERROR:
        errStr = tr("CSync processing step reconcile failed.");
        break;
    case CSYNC_STATUS_PROPAGATE_ERROR:
        errStr = tr("CSync processing step propagate failed.");
        break;
    case CSYNC_STATUS_REMOTE_ACCESS_ERROR:
        errStr = tr("<p>The target directory does not exist.</p><p>Please check the sync setup.</p>");
        break;
    case CSYNC_STATUS_REMOTE_CREATE_ERROR:
    case CSYNC_STATUS_REMOTE_STAT_ERROR:
        errStr = tr("A remote file can not be written. Please check the remote access.");
        break;
    case CSYNC_STATUS_LOCAL_CREATE_ERROR:
    case CSYNC_STATUS_LOCAL_STAT_ERROR:
        errStr = tr("The local filesystem can not be written. Please check permissions.");
        break;
    case CSYNC_STATUS_PROXY_ERROR:
        errStr = tr("CSync failed to connect through a proxy.");
        break;
    case CSYNC_STATUS_PROXY_AUTH_ERROR:
        errStr = tr("CSync could not authenticate at the proxy.");
        break;
    case CSYNC_STATUS_LOOKUP_ERROR:
        errStr = tr("CSync failed to lookup proxy or server.");
        break;
    case CSYNC_STATUS_SERVER_AUTH_ERROR:
        errStr = tr("CSync failed to authenticate at the %1 server.").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_STATUS_CONNECT_ERROR:
        errStr = tr("CSync failed to connect to the network.");
        break;
    case CSYNC_STATUS_TIMEOUT:
        errStr = tr("A network connection timeout happend.");
        break;
    case CSYNC_STATUS_HTTP_ERROR:
        errStr = tr("A HTTP transmission error happened.");
        break;
    case CSYNC_STATUS_PERMISSION_DENIED:
        errStr = tr("CSync failed due to not handled permission deniend.");
        break;
    case CSYNC_STATUS_NOT_FOUND:
        errStr = tr("CSync failed to access "); // filename gets added.
        break;
    case CSYNC_STATUS_FILE_EXISTS:
        errStr = tr("CSync tried to create a directory that already exists.");
        break;
    case CSYNC_STATUS_OUT_OF_SPACE:
        errStr = tr("CSync: No space on %1 server available.").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_STATUS_QUOTA_EXCEEDED:
        errStr = tr("CSync: No space on %1 server available.").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_STATUS_UNSUCCESSFUL:
        errStr = tr("CSync unspecified error.");
        break;
    case CSYNC_STATUS_ABORTED:
        errStr = tr("Aborted by the user");
        break;

    default:
        errStr = tr("An internal error number %1 happened.").arg( (int) err );
    }

    return errStr;

}

bool SyncEngine::checkBlacklisting( SyncFileItem *item )
{
    bool re = false;

    if( !_journal ) {
        qWarning() << "Journal is undefined!";
        return false;
    }

    SyncJournalBlacklistRecord entry = _journal->blacklistEntry(item->_file);
    item->_blacklistedInDb = false;

    // if there is a valid entry in the blacklist table and the retry count is
    // already null or smaller than 0, the file is blacklisted.
    if( entry.isValid() ) {
        item->_blacklistedInDb = true;

        if( entry._retryCount <= 0 ) {
            re = true;
        }

        // if the retryCount is 0, but the etag for downloads or the mtime for uploads
        // has changed, it is tried again
        // note that if the retryCount is -1 we never try again.
        if( entry._retryCount == 0 ) {
            if( item->_direction == SyncFileItem::Up ) { // check the modtime
                if(item->_modtime == 0 || entry._lastTryModtime == 0) {
                    re = false;
                } else {
                    if( item->_modtime != entry._lastTryModtime ) {
                        re = false;
                        qDebug() << item->_file << " is blacklisted, but has changed mtime!";
                    }
                }
            } else {
                // download, check the etag.
                if( item->_etag.isEmpty() || entry._lastTryEtag.isEmpty() ) {
                    qDebug() << item->_file << "one ETag is empty, no blacklisting";
                    return false;
                } else {
                    if( item->_etag != entry._lastTryEtag ) {
                        re = false;
                        qDebug() << item->_file << " is blacklisted, but has changed etag!";
                    }
                }
            }
        }

        if( re ) {
            qDebug() << "Item is on blacklist: " << entry._file << "retries:" << entry._retryCount;
            item->_instruction = CSYNC_INSTRUCTION_IGNORE;
            item->_errorString = tr("The item is not synced because of previous errors: %1").arg(entry._errorString);
        }
    }

    return re;
}

int SyncEngine::treewalkLocal( TREE_WALK_FILE* file, void *data )
{
    return static_cast<SyncEngine*>(data)->treewalkFile( file, false );
}

int SyncEngine::treewalkRemote( TREE_WALK_FILE* file, void *data )
{
    return static_cast<SyncEngine*>(data)->treewalkFile( file, true );
}

int SyncEngine::treewalkFile( TREE_WALK_FILE *file, bool remote )
{
    if( ! file ) return -1;
    SyncFileItem item;
    item._file = QString::fromUtf8( file->path );
    item._originalFile = item._file;
    item._instruction = file->instruction;
    item._direction = SyncFileItem::None;
    item._fileId = file->file_id;

    // record the seen files to be able to clean the journal later
    _seenFiles[item._file] = QString();

    switch(file->error_status) {
    case CSYNC_STATUS_OK:
        break;
    case CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK:
        item._errorString = tr("Symbolic links are not supported in syncing.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST:
        item._errorString = tr("File is listed on the ignore list.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS:
        item._errorString = tr("File contains invalid characters that can not be synced cross platform.");
        break;
    case CYSNC_STATUS_FILE_LOCKED_OR_OPEN:
        item._errorString = QLatin1String("File locked"); // don't translate, internal use!
        break;

    default:
        Q_ASSERT("Non handled error-status");
        /* No error string */
    }

    item._isDirectory = file->type == CSYNC_FTW_TYPE_DIR;
    item._modtime = file->modtime;
    item._etag = file->etag;
    item._size = file->size;
    item._inode = file->inode;

    item._should_update_etag = file->should_update_etag;
    switch( file->type ) {
    case CSYNC_FTW_TYPE_DIR:
        item._type = SyncFileItem::Directory;
        break;
    case CSYNC_FTW_TYPE_FILE:
        item._type = SyncFileItem::File;
        break;
    case CSYNC_FTW_TYPE_SLINK:
        item._type = SyncFileItem::SoftLink;
        break;
    default:
        item._type = SyncFileItem::UnknownType;
    }

    SyncFileItem::Direction dir;

    int re = 0;

    switch(file->instruction) {
    case CSYNC_INSTRUCTION_NONE:
        if (file->should_update_etag && !item._isDirectory) {
            // Update the database now already  (new fileid or etag)
            _journal->setFileRecord(SyncJournalFileRecord(item, _localPath + item._file));
            item._should_update_etag = false;
        }
        if (item._isDirectory && remote) {
            // Because we want still to update etags of directories
            dir = SyncFileItem::None;
        } else {
            // No need to do anything.
            _hasFiles = true;

            emit syncItemDiscovered(item);
            return re;
        }
        break;
    case CSYNC_INSTRUCTION_RENAME:
        dir = !remote ? SyncFileItem::Down : SyncFileItem::Up;
        item._renameTarget = QString::fromUtf8( file->rename_path );
        if (item._isDirectory)
            _renamedFolders.insert(item._file, item._renameTarget);
        break;
    case CSYNC_INSTRUCTION_REMOVE:
        dir = !remote ? SyncFileItem::Down : SyncFileItem::Up;
                break;
    case CSYNC_INSTRUCTION_CONFLICT:
    case CSYNC_INSTRUCTION_IGNORE:
    case CSYNC_INSTRUCTION_ERROR:
        dir = SyncFileItem::None;
        break;
    case CSYNC_INSTRUCTION_EVAL:
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_STAT_ERROR:
    default:
        dir = remote ? SyncFileItem::Down : SyncFileItem::Up;
        break;
    }

    item._direction = dir;
    // check for blacklisting of this item.
    // if the item is on blacklist, the instruction was set to IGNORE
    checkBlacklisting( &item );

    if (file->instruction != CSYNC_INSTRUCTION_IGNORE
        && file->instruction != CSYNC_INSTRUCTION_REMOVE) {
      _hasFiles = true;
    }

    if (!item._isDirectory) {
        _progressInfo._totalFileCount++;
        if (Progress::isSizeDependent(file->instruction)) {
            _progressInfo._totalSize += file->size;
        }
    }
    _needsUpdate = true;

    item.log._etag          = file->etag;
    item.log._fileId        = file->file_id;
    item.log._instruction   = file->instruction;
    item.log._modtime       = file->modtime;
    item.log._size          = file->size;

    item.log._other_etag        = file->other.etag;
    item.log._other_fileId      = file->other.file_id;
    item.log._other_instruction = file->other.instruction;
    item.log._other_modtime     = file->other.modtime;
    item.log._other_size        = file->other.size;

    _syncedItems.append(item);
    emit syncItemDiscovered(item);
    return re;
}

void SyncEngine::handleSyncError(CSYNC *ctx, const char *state) {
    CSYNC_STATUS err = csync_get_status( ctx );
    const char *errMsg = csync_get_status_string( ctx );
    QString errStr = csyncErrorToString(err);
    if( errMsg ) {
        if( !errStr.endsWith(" ")) {
            errStr.append(" ");
        }
        errStr += QString::fromUtf8(errMsg);
    }

    // if there is csyncs url modifier in the error message, replace it.
    if( errStr.contains("ownclouds://") ) errStr.replace("ownclouds://", "https://");
    if( errStr.contains("owncloud://") ) errStr.replace("owncloud://", "http://");

    qDebug() << " #### ERROR during "<< state << ": " << errStr;

    if( CSYNC_STATUS_IS_EQUAL( err, CSYNC_STATUS_ABORTED) ) {
        qDebug() << "Update phase was aborted by user!";
    } else if( CSYNC_STATUS_IS_EQUAL( err, CSYNC_STATUS_SERVICE_UNAVAILABLE ) ||
            CSYNC_STATUS_IS_EQUAL( err, CSYNC_STATUS_CONNECT_ERROR )) {
        emit csyncUnavailable();
    } else {
        emit csyncError(errStr);
    }
    finalize();
}

void SyncEngine::startSync()
{
    if (!_syncMutex.tryLock()) {
        qDebug() << Q_FUNC_INFO << "WARNING: Another sync seems to be running. Not starting a new one.";
        return;
    }

    if( ! _csync_ctx ) {
        qDebug() << "XXXXXXXXXXXXXXXX FAIL: do not have csync_ctx!";
    }

    _syncedItems.clear();
    _needsUpdate = false;

    csync_resume(_csync_ctx);

    if (!_journal->exists()) {
        qDebug() << "=====sync looks new (no DB exists), activating recursive PROPFIND if csync supports it";
        bool no_recursive_propfind = false;
        csync_set_module_property(_csync_ctx, "no_recursive_propfind", &no_recursive_propfind);
    } else {
        // retrieve the file count from the db and close it afterwards because
        // csync_update also opens the database.
        int fileRecordCount = 0;
        fileRecordCount = _journal->getFileRecordCount();
        bool isUpdateFrom_1_5 = _journal->isUpdateFrom_1_5();
        _journal->close();

        if( fileRecordCount == -1 ) {
            qDebug() << "No way to create a sync journal!";
            emit csyncError(tr("Unable to initialize a sync journal."));
            finalize();
            return;
            // database creation error!
        } else if ( fileRecordCount < 50 ) {
            qDebug() << "=====sync DB has only" << fileRecordCount << "items, enable recursive PROPFIND if csync supports it";
            bool no_recursive_propfind = false;
            csync_set_module_property(_csync_ctx, "no_recursive_propfind", &no_recursive_propfind);
        } else {
            qDebug() << "=====sync with existing DB";
        }

        if (fileRecordCount > 1 && isUpdateFrom_1_5) {
            qDebug() << "detected update from 1.5";
            // Disable the read from DB to be sure to re-read all the fileid and etags.
            csync_set_read_from_db(_csync_ctx, false);
        }
    }

    csync_set_module_property(_csync_ctx, "csync_context", _csync_ctx);
    csync_set_userdata(_csync_ctx, this);
    // TODO: This should be a part of this method, but we don't have
    // any way to get "session_key" module property from csync. Had we
    // have it, then we could keep this code and remove it from
    // AbstractCredentials implementations.
    if (Account *account = AccountManager::instance()->account()) {
        account->credentials()->syncContextPreStart(_csync_ctx);
    } else {
        qDebug() << Q_FUNC_INFO << "No default Account object, huh?";
    }
    // if (_lastAuthCookies.length() > 0) {
    //     // Stuff cookies inside csync, then we can avoid the intermediate HTTP 401 reply
    //     // when https://github.com/owncloud/core/pull/4042 is merged.
    //     QString cookiesAsString;
    //     foreach(QNetworkCookie c, _lastAuthCookies) {
    //         cookiesAsString += c.name();
    //         cookiesAsString += '=';
    //         cookiesAsString += c.value();
    //         cookiesAsString += "; ";
    //     }
    //     csync_set_module_property(_csync_ctx, "session_key", cookiesAsString.to
    // }

    // csync_set_auth_callback( _csync_ctx, getauth );
    csync_set_log_callback( csyncLogCatcher );
    //csync_set_log_level( 11 ); don't set the loglevel here, it shall be done by folder.cpp or owncloudcmd.cpp
    int timeout = OwncloudPropagator::httpTimeout();
    csync_set_module_property(_csync_ctx, "timeout", &timeout);


    _stopWatch.start();

    qDebug() << "#### Update start #################################################### >>";

    UpdateJob *job = new UpdateJob(_csync_ctx);
    job->moveToThread(&_thread);
    connect(job, SIGNAL(finished(int)), this, SLOT(slotUpdateFinished(int)));
    QMetaObject::invokeMethod(job, "start", Qt::QueuedConnection);
}

void SyncEngine::slotUpdateFinished(int updateResult)
{
    if (updateResult < 0 ) {
        handleSyncError(_csync_ctx, "csync_update");
        return;
    }
    qDebug() << "<<#### Update end #################################################### " << _stopWatch.addLapTime(QLatin1String("Update Finished"));

    if( csync_reconcile(_csync_ctx) < 0 ) {
        handleSyncError(_csync_ctx, "csync_reconcile");
        return;
    }
    _stopWatch.addLapTime(QLatin1String("Reconcile Finished"));

    _progressInfo = Progress::Info();

    _hasFiles = false;
    bool walkOk = true;
    _seenFiles.clear();

    if( csync_walk_local_tree(_csync_ctx, &treewalkLocal, 0) < 0 ) {
        qDebug() << "Error in local treewalk.";
        walkOk = false;
    }
    if( walkOk && csync_walk_remote_tree(_csync_ctx, &treewalkRemote, 0) < 0 ) {
        qDebug() << "Error in remote treewalk.";
    }

    // Adjust the paths for the renames.
    for (SyncFileItemVector::iterator it = _syncedItems.begin();
            it != _syncedItems.end(); ++it) {
        it->_file = adjustRenamedPath(it->_file);
    }

    // Sanity check
    if (!_journal->isConnected()) {
        qDebug() << "Bailing out, DB failure";
        emit csyncError(tr("Cannot open the sync journal"));
        finalize();
        return;
    }

    // To announce the beginning of the sync
    emit aboutToPropagate(_syncedItems);
    emit transmissionProgress(_progressInfo);

    if (!_hasFiles && !_syncedItems.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "All the files are going to be removed, asking the user";
        bool cancel = false;
        emit aboutToRemoveAllFiles(_syncedItems.first()._direction, &cancel);
        if (cancel) {
            qDebug() << Q_FUNC_INFO << "Abort sync";
            finalize();
            return;
        }
    }

    if (_needsUpdate)
        emit(started());

    ne_session_s *session = 0;
    // that call to set property actually is a get which will return the session
    csync_set_module_property(_csync_ctx, "get_dav_session", &session);
    Q_ASSERT(session);

    _propagator.reset(new OwncloudPropagator (session, _localPath, _remoteUrl, _remotePath,
                                              _journal, &_thread));
    connect(_propagator.data(), SIGNAL(completed(SyncFileItem)),
            this, SLOT(slotJobCompleted(SyncFileItem)));
    connect(_propagator.data(), SIGNAL(progress(SyncFileItem,quint64)),
            this, SLOT(slotProgress(SyncFileItem,quint64)));
    connect(_propagator.data(), SIGNAL(adjustTotalTransmissionSize(qint64)), this, SLOT(slotAdjustTotalTransmissionSize(qint64)));
    connect(_propagator.data(), SIGNAL(finished()), this, SLOT(slotFinished()));

    setNetworkLimits();

    _propagator->start(_syncedItems);
}

void SyncEngine::setNetworkLimits()
{
    MirallConfigFile cfg;

    if( !_propagator ) return;

    int downloadLimit = 0;
    if (cfg.useDownloadLimit()) {
        downloadLimit = cfg.downloadLimit() * 1000;
    }
    _propagator->_downloadLimit = downloadLimit;

    int uploadLimit = -75; // 75%
    int useUpLimit = cfg.useUploadLimit();
    if ( useUpLimit >= 1) {
        uploadLimit = cfg.uploadLimit() * 1000;
    } else if (useUpLimit == 0) {
        uploadLimit = 0;
    }
    _propagator->_uploadLimit = uploadLimit;

    int propDownloadLimit = _propagator->_downloadLimit
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
            .load()
#endif
            ;
    int propUploadLimit = _propagator->_uploadLimit
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
            .load()
#endif
            ;

    if( propDownloadLimit != 0 || propUploadLimit != 0 ) {
        qDebug() << " N------N Network Limits (down/up) " << propDownloadLimit << propUploadLimit;
    }
}

void SyncEngine::slotJobCompleted(const SyncFileItem &item)
{
    qDebug() << Q_FUNC_INFO << item._file << item._status << item._errorString;

    /* Update the _syncedItems vector */
    int idx = _syncedItems.indexOf(item);
    if (idx >= 0) {
        _syncedItems[idx]._instruction = item._instruction;
        _syncedItems[idx]._errorString = item._errorString;
        _syncedItems[idx]._status = item._status;

        _syncedItems[idx]._requestDuration = item._requestDuration;
        _syncedItems[idx]._responseTimeStamp = item._responseTimeStamp;
    } else {
        qWarning() << Q_FUNC_INFO << "Could not find index in synced items!";

    }

    _progressInfo.setProgressComplete(item);

    if (item._status == SyncFileItem::FatalError) {
        emit csyncError(item._errorString);
    }

    emit transmissionProgress(_progressInfo);
    emit jobCompleted(item);
}

void SyncEngine::slotFinished()
{
    // emit the treewalk results.
    if( ! _journal->postSyncCleanup( _seenFiles ) ) {
        qDebug() << "Cleaning of synced ";
    }
    _journal->commit("All Finished.", false);
    emit treeWalkResult(_syncedItems);
    finalize();
}

void SyncEngine::finalize()
{
    csync_commit(_csync_ctx);

    qDebug() << "CSync run took " << _stopWatch.addLapTime(QLatin1String("Sync Finished"));
    _stopWatch.stop();

    _propagator.reset(0);
    _syncMutex.unlock();
    _thread.quit();
    _thread.wait();
    emit finished();
}

void SyncEngine::slotProgress(const SyncFileItem& item, quint64 current)
{
    _progressInfo.setProgressItem(item, current);
    emit transmissionProgress(_progressInfo);
}


void SyncEngine::slotAdjustTotalTransmissionSize(qint64 change)
{
    _progressInfo._totalSize += change;
}

/* Given a path on the remote, give the path as it is when the rename is done */
QString SyncEngine::adjustRenamedPath(const QString& original)
{
    int slashPos = original.size();
    while ((slashPos = original.lastIndexOf('/' , slashPos - 1)) > 0) {
        QHash< QString, QString >::const_iterator it = _renamedFolders.constFind(original.left(slashPos));
        if (it != _renamedFolders.constEnd()) {
            return *it + original.mid(slashPos);
        }
    }
    return original;
}

void SyncEngine::abort()
{
    csync_request_abort(_csync_ctx);
    if(_propagator)
        _propagator->abort();
}

} // ns Mirall

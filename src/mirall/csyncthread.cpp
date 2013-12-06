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

#include "mirall/csyncthread.h"
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
#include <QApplication>
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
QMutex CSyncThread::_mutex;
QMutex CSyncThread::_syncMutex;

CSyncThread::CSyncThread(CSYNC *csync, const QString &localPath, const QString &remotePath, SyncJournalDb *journal)
{
    _mutex.lock();
    _localPath = localPath;
    _remotePath = remotePath;
    _csync_ctx = csync;
    _journal = journal;
    _mutex.unlock();
    qRegisterMetaType<SyncFileItem>("SyncFileItem");
    qRegisterMetaType<SyncFileItem::Status>("SyncFileItem::Status");
}

CSyncThread::~CSyncThread()
{

}

//Convert an error code from csync to a user readable string.
// Keep that function thread safe as it can be called from the sync thread or the main thread
QString CSyncThread::csyncErrorToString(CSYNC_STATUS err)
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
        errStr = tr("CSync failed to load the state db.");
        break;
    case CSYNC_STATUS_STATEDB_WRITE_ERROR:
        errStr = tr("CSync failed to write the state db.");
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
        errStr = tr("An internal error number %1 happend.").arg( (int) err );
    }

    return errStr;

}

bool CSyncThread::checkBlacklisting( SyncFileItem *item )
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
            if( item->_dir == SyncFileItem::Up ) { // check the modtime
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
            item->_errorString = tr("The item is not synced because of previous errors.");
            slotProgress( Progress::SoftError, *item );
        }
    }

    return re;
}

int CSyncThread::treewalkLocal( TREE_WALK_FILE* file, void *data )
{
    return static_cast<CSyncThread*>(data)->treewalkFile( file, false );
}

int CSyncThread::treewalkRemote( TREE_WALK_FILE* file, void *data )
{
    return static_cast<CSyncThread*>(data)->treewalkFile( file, true );
}

int CSyncThread::treewalkFile( TREE_WALK_FILE *file, bool remote )
{
    if( ! file ) return -1;
    SyncFileItem item;
    item._file = QString::fromUtf8( file->path );
    item._originalFile = item._file;
    item._instruction = file->instruction;
    item._dir = SyncFileItem::None;
    item._fileId = QString::fromUtf8(file->file_id);

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
    default:
        Q_ASSERT("Non handled error-status");
        /* No error string */
    }

    item._isDirectory = file->type == CSYNC_FTW_TYPE_DIR;
    item._modtime = file->modtime;
    item._etag = file->etag;
    item._size = file->size;
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
        break;
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_CONFLICT:
    case CSYNC_INSTRUCTION_RENAME:
    case CSYNC_INSTRUCTION_REMOVE:
        _progressInfo.overall_file_count++;
        _progressInfo.overall_transmission_size += file->size;
        //fall trough
    default:
        _needsUpdate = true;
    }
    switch(file->instruction) {
    case CSYNC_INSTRUCTION_UPDATED:
        // We need to update the database.
        _journal->setFileRecord(SyncJournalFileRecord(item, _localPath + item._file));
        item._instruction = CSYNC_INSTRUCTION_NONE;
        // fall trough
    case CSYNC_INSTRUCTION_NONE:
        if (item._isDirectory && remote) {
            // Because we want still to update etags of directories
            dir = SyncFileItem::None;
        } else {
            // No need to do anything.
            _hasFiles = true;

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
        //
        slotProgress(Progress::SoftError, item, 0, 0);
        dir = SyncFileItem::None;
        break;
    case CSYNC_INSTRUCTION_EVAL:
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_STAT_ERROR:
    case CSYNC_INSTRUCTION_DELETED:
    default:
        dir = remote ? SyncFileItem::Down : SyncFileItem::Up;
        break;
    }

    item._dir = dir;
    // check for blacklisting of this item.
    // if the item is on blacklist, the instruction was set to IGNORE
    checkBlacklisting( &item );

    if (file->instruction != CSYNC_INSTRUCTION_IGNORE
        && file->instruction != CSYNC_INSTRUCTION_REMOVE) {
      _hasFiles = true;
    }
    _syncedItems.append(item);

    return re;
}

void CSyncThread::handleSyncError(CSYNC *ctx, const char *state) {
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
    csync_commit(_csync_ctx);
    emit finished();
    _syncMutex.unlock();
    thread()->quit();
}

void CSyncThread::startSync()
{
    if (!_syncMutex.tryLock()) {
        qDebug() << Q_FUNC_INFO << "WARNING: Another sync seems to be running. Not starting a new one.";
        return;
    }

    if( ! _csync_ctx ) {
        qDebug() << "XXXXXXXXXXXXXXXX FAIL: do not have csync_ctx!";
    }
    qDebug() << Q_FUNC_INFO << "Sync started";


    qDebug() << "starting to sync " << qApp->thread() << QThread::currentThread();
    _syncedItems.clear();

    _mutex.lock();
    _needsUpdate = false;
    if (!_abortRequested.fetchAndAddRelease(0)) {
        csync_resume(_csync_ctx);
    }
    _mutex.unlock();


    // maybe move this somewhere else where it can influence a running sync?
    MirallConfigFile cfg;

    if (!_journal->exists()) {
        qDebug() << "=====sync looks new (no DB exists), activating recursive PROPFIND if csync supports it";
        bool no_recursive_propfind = false;
        csync_set_module_property(_csync_ctx, "no_recursive_propfind", &no_recursive_propfind);
    } else {
        // retrieve the file count from the db and close it afterwards because
        // csync_update also opens the database.
        int fileRecordCount = 0;
        fileRecordCount = _journal->getFileRecordCount();
        _journal->close();

        if( fileRecordCount == -1 ) {
            qDebug() << "No way to create a sync journal!";
            emit csyncError(tr("Unable to initialize a sync journal."));

            csync_commit(_csync_ctx);
            emit finished();
            _syncMutex.unlock();
            thread()->quit();

            return;
            // database creation error!
        } else if ( fileRecordCount < 50 ) {
            qDebug() << "=====sync DB has only" << fileRecordCount << "items, enable recursive PROPFIND if csync supports it";
            bool no_recursive_propfind = false;
            csync_set_module_property(_csync_ctx, "no_recursive_propfind", &no_recursive_propfind);
        } else {
            qDebug() << "=====sync with existing DB";
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
    csync_set_log_level( 11 );

    _syncTime.start();

    QElapsedTimer updateTime;
    updateTime.start();
    qDebug() << "#### Update start #################################################### >>";

    if( csync_update(_csync_ctx) < 0 ) {
        handleSyncError(_csync_ctx, "csync_update");
        return;
    }
    qDebug() << "<<#### Update end #################################################### " << updateTime.elapsed();

    if( csync_reconcile(_csync_ctx) < 0 ) {
        handleSyncError(_csync_ctx, "csync_reconcile");
        return;
    }

    slotProgress(Progress::StartSync, SyncFileItem(), 0, 0);

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

    if (!_hasFiles && !_syncedItems.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "All the files are going to be removed, asking the user";
        bool cancel = false;
        emit aboutToRemoveAllFiles(_syncedItems.first()._dir, &cancel);
        if (cancel) {
            qDebug() << Q_FUNC_INFO << "Abort sync";
            return;
        }
    }

    if (_needsUpdate)
        emit(started());

    ne_session_s *session = 0;
    // that call to set property actually is a get which will return the session
    // FIXME add a csync_get_module_property to csync

    csync_set_module_property(_csync_ctx, "get_dav_session", &session);
    Q_ASSERT(session);

    _propagator.reset(new OwncloudPropagator (session, _localPath, _remotePath,
                                              _journal, &_abortRequested));
    connect(_propagator.data(), SIGNAL(completed(SyncFileItem)),
            this, SLOT(transferCompleted(SyncFileItem)), Qt::QueuedConnection);
    connect(_propagator.data(), SIGNAL(progress(Progress::Kind,SyncFileItem,quint64,quint64)),
            this, SLOT(slotProgress(Progress::Kind,SyncFileItem,quint64,quint64)));
    connect(_propagator.data(), SIGNAL(finished()), this, SLOT(slotFinished()));

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

    _propagator->start(_syncedItems);
}

void CSyncThread::transferCompleted(const SyncFileItem &item)
{
    qDebug() << Q_FUNC_INFO << item._file << item._status << item._errorString;

    /* Update the _syncedItems vector */
    int idx = _syncedItems.indexOf(item);
    if (idx >= 0) {
        _syncedItems[idx]._instruction = item._instruction;
        _syncedItems[idx]._errorString = item._errorString;
        _syncedItems[idx]._status = item._status;

    } else {
        qWarning() << Q_FUNC_INFO << "Could not find index in synced items!";
    }

    if (item._status == SyncFileItem::FatalError) {
        emit csyncError(item._errorString);
    }
}

void CSyncThread::slotFinished()
{
    // emit the treewalk results.
    if( ! _journal->postSyncCleanup( _seenFiles ) ) {
        qDebug() << "Cleaning of synced ";
    }
    _journal->commit("All Finished.", false);
    emit treeWalkResult(_syncedItems);

    csync_commit(_csync_ctx);

    qDebug() << "CSync run took " << _syncTime.elapsed() << " Milliseconds";
    slotProgress(Progress::EndSync,SyncFileItem(), 0 , 0);
    emit finished();
    _propagator.reset(0);
    _syncMutex.unlock();
    thread()->quit();
}

void CSyncThread::progressProblem(Progress::Kind kind, const SyncFileItem& item)
{
    Progress::SyncProblem problem;

    problem.kind = kind;
    problem.current_file = item._file;
    problem.error_message = item._errorString;
    problem.error_code = item._httpErrorCode;
    problem.timestamp =  QDateTime::currentDateTime();

    // connected to something in folder.
    emit transmissionProblem( problem );
}

void CSyncThread::slotProgress(Progress::Kind kind, const SyncFileItem& item, quint64 curr, quint64 total)
{
    if( Progress::isErrorKind(kind) ) {
        progressProblem(kind, item);
        return;
    }

    if( kind == Progress::StartSync ) {
        QMutexLocker lock(&_mutex);
        _currentFileNo = 0;
        _lastOverallBytes = 0;
    }
    if( kind == Progress::StartDelete ||
            kind == Progress::StartDownload ||
            kind == Progress::StartRename ||
            kind == Progress::StartUpload ) {
        QMutexLocker lock(&_mutex);
        _currentFileNo += 1;
    }

    if( kind == Progress::EndUpload ||
            kind == Progress::EndDownload ||
            kind == Progress::EndRename ||
            kind == Progress::EndDelete ) {
        QMutexLocker lock(&_mutex);
        _lastOverallBytes += total;
        curr = 0;
    }

    Progress::Info pInfo(_progressInfo);
    pInfo.kind                  = kind;
    pInfo.current_file          = item._file;
    pInfo.rename_target         = item._renameTarget;
    pInfo.file_size             = total;
    pInfo.current_file_bytes    = curr;
    pInfo.current_file_no       = _currentFileNo;
    pInfo.timestamp             = QDateTime::currentDateTime();
    pInfo.overall_current_bytes = _lastOverallBytes + curr;
    // Connect to something in folder!
    emit transmissionProgress( pInfo );
}

/* Given a path on the remote, give the path as it is when the rename is done */
QString CSyncThread::adjustRenamedPath(const QString& original)
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

void CSyncThread::abort()
{
    QMutexLocker locker(&_mutex);
    csync_request_abort(_csync_ctx);
    _abortRequested = true;
}


} // ns Mirall

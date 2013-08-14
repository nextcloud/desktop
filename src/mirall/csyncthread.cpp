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
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"
#include "mirall/logger.h"
#include "mirall/owncloudinfo.h"
#include "owncloudpropagator.h"
#include "progressdatabase.h"
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

/* static variables to hold the credentials */

QMutex CSyncThread::_mutex;
QMutex CSyncThread::_syncMutex;

CSyncThread::CSyncThread(CSYNC *csync, const QString &localPath, const QString &remotePath)
{
    _mutex.lock();
    _localPath = localPath;
    _remotePath = remotePath;
    _csync_ctx = csync;
    _mutex.unlock();
    qRegisterMetaType<SyncFileItem>("SyncFileItem");
    qRegisterMetaType<CSYNC_ERROR_CODE>("CSYNC_ERROR_CODE");
}

CSyncThread::~CSyncThread()
{

}

//Convert an error code from csync to a user readable string.
// Keep that function thread safe as it can be called from the sync thread or the main thread
QString CSyncThread::csyncErrorToString( CSYNC_ERROR_CODE err )
{
    QString errStr;

    switch( err ) {
    case CSYNC_ERR_NONE:
        errStr = tr("Success.");
        break;
    case CSYNC_ERR_LOG:
        errStr = tr("CSync Logging setup failed.");
        break;
    case CSYNC_ERR_LOCK:
        errStr = tr("CSync failed to create a lock file.");
        break;
    case CSYNC_ERR_STATEDB_LOAD:
        errStr = tr("CSync failed to load the state db.");
        break;
    case CSYNC_ERR_MODULE:
        errStr = tr("<p>The %1 plugin for csync could not be loaded.<br/>Please verify the installation!</p>").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_ERR_TIMESKEW:
        errStr = tr("The system time on this client is different than the system time on the server. "
                    "Please use a time synchronization service (NTP) on the server and client machines "
                    "so that the times remain the same.");
        break;
    case CSYNC_ERR_FILESYSTEM:
        errStr = tr("CSync could not detect the filesystem type.");
        break;
    case CSYNC_ERR_TREE:
        errStr = tr("CSync got an error while processing internal trees.");
        break;
    case CSYNC_ERR_MEM:
        errStr = tr("CSync failed to reserve memory.");
        break;
    case CSYNC_ERR_PARAM:
        errStr = tr("CSync fatal parameter error.");
        break;
    case CSYNC_ERR_UPDATE:
        errStr = tr("CSync processing step update failed.");
        break;
    case CSYNC_ERR_RECONCILE:
        errStr = tr("CSync processing step reconcile failed.");
        break;
    case CSYNC_ERR_PROPAGATE:
        errStr = tr("CSync processing step propagate failed.");
        break;
    case CSYNC_ERR_ACCESS_FAILED:
        errStr = tr("<p>The target directory does not exist.</p><p>Please check the sync setup.</p>");
        break;
    case CSYNC_ERR_REMOTE_CREATE:
    case CSYNC_ERR_REMOTE_STAT:
        errStr = tr("A remote file can not be written. Please check the remote access.");
        break;
    case CSYNC_ERR_LOCAL_CREATE:
    case CSYNC_ERR_LOCAL_STAT:
        errStr = tr("The local filesystem can not be written. Please check permissions.");
        break;
    case CSYNC_ERR_PROXY:
        errStr = tr("CSync failed to connect through a proxy.");
        break;
    case CSYNC_ERR_LOOKUP:
        errStr = tr("CSync failed to lookup proxy or server.");
        break;
    case CSYNC_ERR_AUTH_SERVER:
        errStr = tr("CSync failed to authenticate at the %1 server.").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_ERR_AUTH_PROXY:
        errStr = tr("CSync failed to authenticate at the proxy.");
        break;
    case CSYNC_ERR_CONNECT:
        errStr = tr("CSync failed to connect to the network.");
        break;
    case CSYNC_ERR_TIMEOUT:
        errStr = tr("A network connection timeout happend.");
        break;
    case CSYNC_ERR_HTTP:
        errStr = tr("A HTTP transmission error happened.");
        break;
    case CSYNC_ERR_PERM:
        errStr = tr("CSync failed due to not handled permission deniend.");
        break;
    case CSYNC_ERR_NOT_FOUND:
        errStr = tr("CSync failed to find a specific file.");
        break;
    case CSYNC_ERR_EXISTS:
        errStr = tr("CSync tried to create a directory that already exists.");
        break;
    case CSYNC_ERR_NOSPC:
        errStr = tr("CSync: No space on %1 server available.").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_ERR_UNSPEC:
        errStr = tr("CSync unspecified error.");

    default:
        errStr = tr("An internal error number %1 happend.").arg( (int) err );
    }

    return errStr;

}

int CSyncThread::treewalkLocal( TREE_WALK_FILE* file, void *data )
{
    return static_cast<CSyncThread*>(data)->treewalkFile( file, false );
}

int CSyncThread::treewalkRemote( TREE_WALK_FILE* file, void *data )
{
    return static_cast<CSyncThread*>(data)->treewalkFile( file, true );
}

int CSyncThread::walkFinalize(TREE_WALK_FILE* file, void *data )
{
    return static_cast<CSyncThread*>(data)->treewalkFinalize( file);
}

int CSyncThread::treewalkFile( TREE_WALK_FILE *file, bool remote )
{
    if( ! file ) return -1;
    SyncFileItem item;
    item._file = QString::fromUtf8( file->path );
    item._originalFile = file->path;
    item._instruction = file->instruction;
    item._dir = SyncFileItem::None;
    item._isDirectory = file->type == CSYNC_FTW_TYPE_DIR;
    item._modtime = file->modtime;
    item._etag = file->md5;
    item._size = file->size;

    SyncFileItem::Direction dir;

    int re = 0;

    if (file->instruction != CSYNC_INSTRUCTION_IGNORE
        && file->instruction != CSYNC_INSTRUCTION_REMOVE) {
      _hasFiles = true;
    }

    switch(file->instruction) {
    case CSYNC_INSTRUCTION_NONE:
    case CSYNC_INSTRUCTION_IGNORE:
        break;
    default:
        if (!_needsUpdate)
            _needsUpdate = true;
    }
    switch(file->instruction) {
    case CSYNC_INSTRUCTION_NONE:
        // No need to do anything.
        return re;
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
        _progressInfo.overall_file_count++;
        _progressInfo.overall_transmission_size += file->size;
        //fall trough
    case CSYNC_INSTRUCTION_IGNORE:
    case CSYNC_INSTRUCTION_ERROR:
        dir = SyncFileItem::None;
        break;
    case CSYNC_INSTRUCTION_EVAL:
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_SYNC:
        _progressInfo.overall_file_count++;
        _progressInfo.overall_transmission_size += file->size;
        //fall trough
    case CSYNC_INSTRUCTION_STAT_ERROR:
    case CSYNC_INSTRUCTION_DELETED:
    case CSYNC_INSTRUCTION_UPDATED:
    default:
        dir = remote ? SyncFileItem::Down : SyncFileItem::Up;
        break;
    }

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

    item._dir = dir;
    _syncedItems.append(item);

    return re;
}

int CSyncThread::treewalkFinalize(TREE_WALK_FILE* file)
{
    if (file->instruction == CSYNC_INSTRUCTION_IGNORE)
        return 0;

    // Update the instruction and etag in the csync rb_tree so it is saved on the database
    QHash<QByteArray, Action>::const_iterator action = _performedActions.constFind(file->path);
    if (action != _performedActions.constEnd()) {
        if (file->instruction != CSYNC_INSTRUCTION_NONE) {
            // it is NONE if we are in the wrong tree (remote vs. local)

            qDebug() << "UPDATING " << file->path << action->instruction;

            file->instruction = action->instruction;
        }

        if (!action->etag.isNull()) {
            // Update the etag even for INSTRUCTION_NONE (eg. renames)
            file->md5 = action->etag.constData();
        }
    }
    return 0;
}

void CSyncThread::handleSyncError(CSYNC *ctx, const char *state) {
    CSYNC_ERROR_CODE err = csync_get_error( ctx );
    const char *errMsg = csync_get_error_string( ctx );
    QString errStr = csyncErrorToString(err);
    if( errMsg ) {
        errStr += QLatin1String("<br/>");
        errStr += QString::fromUtf8(errMsg);
    }
    qDebug() << " #### ERROR during "<< state << ": " << errStr;
    switch (err) {
    case CSYNC_ERR_SERVICE_UNAVAILABLE:
    case CSYNC_ERR_CONNECT:
        emit csyncUnavailable();
        break;
    default:
        emit csyncError(errStr);
    }
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
    _mutex.unlock();


    // maybe move this somewhere else where it can influence a running sync?
    MirallConfigFile cfg;


    csync_set_module_property(_csync_ctx, "csync_context", _csync_ctx);
    csync_set_userdata(_csync_ctx, this);

    // TODO: This should be a part of this method, but we don't have
    // any way to get "session_key" module property from csync. Had we
    // have it, then we could keep this code and remove it from
    // AbstractCredentials implementations.
    cfg.getCredentials()->syncContextPreStart(_csync_ctx);
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

    _progressInfo = Progress::Info();

    _hasFiles = false;
    bool walkOk = true;
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

    qSort(_syncedItems);

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
    csync_set_module_property(_csync_ctx, "get_dav_session", &session);
    Q_ASSERT(session);

    _progressDataBase.load(_localPath);
    _propagator.reset(new OwncloudPropagator (session, _localPath, _remotePath, &_progressDataBase));
    connect(_propagator.data(), SIGNAL(completed(SyncFileItem, CSYNC_ERROR_CODE)),
            this, SLOT(transferCompleted(SyncFileItem, CSYNC_ERROR_CODE)), Qt::QueuedConnection);
    connect(_propagator.data(), SIGNAL(progress(Progress::Kind,QString,quint64,quint64)),
            this, SLOT(slotProgress(Progress::Kind,QString,quint64,quint64)));
    _iterator = 0;

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

    slotProgress(Progress::StartSync, QString(), 0, 0);

    startNextTransfer();
}

void CSyncThread::transferCompleted(const SyncFileItem &item, CSYNC_ERROR_CODE error)
{
    Action a;
    a.instruction = item._instruction;

    // if the propagator had an error for a file, put the error string into the synced item
    if( error != CSYNC_ERR_NONE
            || a.instruction == CSYNC_INSTRUCTION_ERROR) {

        // Search for the item in the starting from _iterator because it should be a bit before it.
        // This works because SyncFileItem::operator== only compare the file name;
        int idx = _syncedItems.lastIndexOf(item, _iterator);
        if (idx >= 0) {
            _syncedItems[idx]._instruction = CSYNC_INSTRUCTION_ERROR;
            _syncedItems[idx]._errorString = csyncErrorToString( error );
            _syncedItems[idx]._errorDetail = item._errorDetail;
            _syncedItems[idx]._httpCode    = item._httpCode;
            qDebug() << "File " << item._file << " propagator error " << _syncedItems[idx]._errorString
                     << "(" << item._errorString << ")";
        }

        if (item._isDirectory && item._instruction == CSYNC_INSTRUCTION_REMOVE
                && a.instruction == CSYNC_INSTRUCTION_DELETED) {
            _lastDeleted = item._file;
        } else {
            _lastDeleted.clear();
        }
    }

    a.etag = item._etag;
    _performedActions.insert(item._originalFile, a);

    if (item._instruction == CSYNC_INSTRUCTION_RENAME) {
        if (a.instruction == CSYNC_INSTRUCTION_DELETED) {
            // we should update the etag on the destination as well
            a.instruction = CSYNC_INSTRUCTION_NONE;
        } else { // ERROR
            a.instruction = CSYNC_INSTRUCTION_ERROR;
        }
        _performedActions.insert(item._renameTarget.toUtf8(), a);
    }

    if (!item._isDirectory && a.instruction == CSYNC_INSTRUCTION_UPDATED) {
        slotProgress((item._dir != SyncFileItem::Up) ? Progress::EndDownload : Progress::EndUpload,
                     item._file, item._size, item._size);
        _progressInfo.current_file_no++;
        _progressInfo.overall_current_bytes += item._size;
    }

    startNextTransfer();
}

void CSyncThread::startNextTransfer()
{
    while (_iterator < _syncedItems.size() && !_propagator->_hasFatalError) {
        const SyncFileItem &item = _syncedItems.at(_iterator);
        ++_iterator;
        if (!_lastDeleted.isEmpty() && item._file.startsWith(_lastDeleted)
                && item._instruction == CSYNC_INSTRUCTION_REMOVE) {
            // If the item's name starts with the name of the previously deleted directory, we
            // can assume this file was already destroyed by the previous recursive call.
            Action a;
            a.instruction = CSYNC_INSTRUCTION_DELETED;
            _performedActions.insert(item._originalFile, a);
            continue;
        }
        _propagator->_etag.clear(); // FIXME : set to the right one

        if (item._instruction == CSYNC_INSTRUCTION_SYNC || item._instruction == CSYNC_INSTRUCTION_NEW
                || item._instruction == CSYNC_INSTRUCTION_CONFLICT) {
            slotProgress((item._dir != SyncFileItem::Up) ? Progress::StartDownload : Progress::StartUpload,
                         item._file, 0, item._size);
        }

        _propagator->propagate(item);
        return; //propagate is async.
    }

    // Everything is finished.
    _progressDataBase.save(_localPath);

    if( csync_walk_local_tree(_csync_ctx, &walkFinalize, 0) < 0 ||
        csync_walk_remote_tree( _csync_ctx, &walkFinalize, 0 ) < 0 ) {
        qDebug() << "Error in finalize treewalk.";
    } else {
    // emit the treewalk results.
        emit treeWalkResult(_syncedItems);
    }

    csync_commit(_csync_ctx);

    qDebug() << "CSync run took " << _syncTime.elapsed() << " Milliseconds";
    slotProgress(Progress::EndSync,QString(), 0 , 0);
    emit finished();
    _propagator.reset(0);
    _syncMutex.unlock();
}

Progress::Kind CSyncThread::csyncToProgressKind( enum csync_notify_type_e kind )
{
    Progress::Kind pKind = Progress::Invalid;

    switch(kind) {
    case CSYNC_NOTIFY_INVALID:
        pKind = Progress::Invalid;
        break;
    case CSYNC_NOTIFY_START_SYNC_SEQUENCE:
        pKind = Progress::StartSync;
        break;
    case CSYNC_NOTIFY_START_DOWNLOAD:
        pKind = Progress::StartDownload;
        break;
    case CSYNC_NOTIFY_START_UPLOAD:
        pKind = Progress::StartUpload;
        break;
    case CSYNC_NOTIFY_PROGRESS:
        pKind = Progress::Context;
        break;
    case CSYNC_NOTIFY_FINISHED_DOWNLOAD:
        pKind = Progress::EndDownload;
        break;
    case CSYNC_NOTIFY_FINISHED_UPLOAD:
        pKind = Progress::EndUpload;
        break;
    case CSYNC_NOTIFY_FINISHED_SYNC_SEQUENCE:
        pKind = Progress::EndSync;
        break;
    case CSYNC_NOTIFY_START_DELETE:
        pKind = Progress::StartDelete;
        break;
    case CSYNC_NOTIFY_END_DELETE:
        pKind = Progress::EndDelete;
        break;
    case CSYNC_NOTIFY_ERROR:
        pKind = Progress::Error;
        break;
    default:
        pKind = Progress::Invalid;
        break;
    }
    return pKind;
}

void CSyncThread::slotProgress(Progress::Kind kind, const QString &file, quint64 curr, quint64 total)
{
    Progress::Info pInfo = _progressInfo;

    pInfo.kind                  = kind;
    pInfo.current_file          = file;
    pInfo.file_size             = total;
    pInfo.current_file_bytes    = curr;

    pInfo.overall_current_bytes += curr;
    pInfo.timestamp = QDateTime::currentDateTime();

    // Connect to something in folder!
    transmissionProgress( pInfo );
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
} // ns Mirall

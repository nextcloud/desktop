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

CSyncThread::CSyncThread(CSYNC *csync)
{
    _mutex.lock();
    _csync_ctx = csync;
    _mutex.unlock();
}

CSyncThread::~CSyncThread()
{

}

QString CSyncThread::csyncErrorToString( CSYNC_ERROR_CODE err, const char *errString )
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
        // this is critical. The database has to be removed.
        emit wipeDb();
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

    if( errString ) {
        errStr += tr("<br/>Backend Message: ")+QString::fromUtf8(errString);
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
    return static_cast<CSyncThread*>(data)->treewalkError( file);
}

int CSyncThread::treewalkFile( TREE_WALK_FILE *file, bool remote )
{
    if( ! file ) return -1;
    SyncFileItem item;
    item._file = QString::fromUtf8( file->path );
    item._instruction = file->instruction;
    item._dir = SyncFileItem::None;

    SyncFileItem::Direction dir;

    int re = 0;

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
    case CSYNC_INSTRUCTION_DELETED:
    case CSYNC_INSTRUCTION_UPDATED:
    default:
        dir = remote ? SyncFileItem::Down : SyncFileItem::Up;
        break;
    }

    item._dir = dir;
    _mutex.lock();
    _syncedItems.append(item);
    _mutex.unlock();

    return re;
}

int CSyncThread::treewalkError(TREE_WALK_FILE* file)
{
    SyncFileItem item;
    item._file= QString::fromUtf8(file->path);
    int indx = _syncedItems.indexOf(item);

    if ( indx == -1 )
        return 0;

    if( file &&
        file->instruction == CSYNC_INSTRUCTION_STAT_ERROR ||
        file->instruction == CSYNC_INSTRUCTION_ERROR ) {
        _mutex.lock();
        _syncedItems[indx]._instruction = file->instruction;
        _mutex.unlock();
    }

    return 0;
}

struct CSyncRunScopeHelper {
    CSyncRunScopeHelper(CSYNC *ctx, CSyncThread *parent)
        : _ctx(ctx), _parent(parent)
    {
        _t.start();
    }
    ~CSyncRunScopeHelper() {
        csync_commit(_ctx);

        qDebug() << "CSync run took " << _t.elapsed() << " Milliseconds";
        emit(_parent->finished());
        _parent->_syncMutex.unlock();
    }
    CSYNC *_ctx;
    QTime _t;
    CSyncThread *_parent;
};

void CSyncThread::handleSyncError(CSYNC *ctx, const char *state) {
    CSYNC_ERROR_CODE err = csync_get_error( ctx );
    const char *errMsg = csync_get_error_string( ctx );
    QString errStr = csyncErrorToString(err, errMsg);
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

    _mutex.lock();
    _syncedItems.clear();
    _needsUpdate = false;
    _mutex.unlock();

    // cleans up behind us and emits finished() to ease error handling
    CSyncRunScopeHelper helper(_csync_ctx, this);

    csync_set_userdata(_csync_ctx, this);

    // csync_set_auth_callback( _csync_ctx, getauth );
    csync_set_progress_callback( _csync_ctx, progress );

    qDebug() << "#### Update start #################################################### >>";
    if( csync_update(_csync_ctx) < 0 ) {
        handleSyncError(_csync_ctx, "csync_update");
        return;
    }
    qDebug() << "<<#### Update end ###########################################################";

    if( csync_reconcile(_csync_ctx) < 0 ) {
        handleSyncError(_csync_ctx, "cysnc_reconcile");
        return;
    }

    bool walkOk = true;
    if( csync_walk_local_tree(_csync_ctx, &treewalkLocal, 0) < 0 ) {
        qDebug() << "Error in local treewalk.";
        walkOk = false;
    }
    if( walkOk && csync_walk_remote_tree(_csync_ctx, &treewalkRemote, 0) < 0 ) {
        qDebug() << "Error in remote treewalk.";
    }

    if (_needsUpdate)
        emit(started());

    if( csync_propagate(_csync_ctx) < 0 ) {
        handleSyncError(_csync_ctx, "cysnc_reconcile");
        return;
    }

    if( walkOk ) {
        if( csync_walk_local_tree(_csync_ctx, &walkFinalize, 0) < 0 ||
            csync_walk_remote_tree( _csync_ctx, &walkFinalize, 0 ) < 0 ) {
            qDebug() << "Error in finalize treewalk.";
        } else {
        // emit the treewalk results.
            emit treeWalkResult(_syncedItems);
        }
    }
    qDebug() << Q_FUNC_INFO << "Sync finished";
}

void CSyncThread::progress(const char *remote_url, enum csync_notify_type_e kind,
                                        long long o1, long long o2, void *userdata)
{
    (void) o1; (void) o2;
    if (kind == CSYNC_NOTIFY_FINISHED_DOWNLOAD) {
        QString path = QUrl::fromEncoded(remote_url).toString();
        CSyncThread *thread = static_cast<CSyncThread*>(userdata);
        thread->fileReceived(path);
    }
}


}
